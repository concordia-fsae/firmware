/*
 * FreeRTOS_SWI.c
 * FreeRTOS Software Interrupt implementation
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "FreeRTOS_SWI.h"

// FreeRTOS includes
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

// Codebase includes
#include "FreeRTOS_types.h"

#include "SystemConfig.h"
#include "Utility.h"

#include <stdlib.h>
#include <string.h>


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

// module struct
typedef struct
{
    RTOS_swiHandle_T   swiTable[RTOS_SWI_PRI_COUNT][RTOS_SWI_MAX_PER_PRI]; // table containing all SWIs
    RTOS_taskDesc_t    swiTasks[RTOS_SWI_PRI_COUNT];                       // task descriptions
    EventGroupHandle_t swiEvents[RTOS_SWI_PRI_COUNT];                      // event group per priority
    uint8_t            swiCountPerPri[RTOS_SWI_PRI_COUNT];                 // count of SWIs defined in each priority
} rtos_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static rtos_S      rtos;
static const char* taskNames[RTOS_SWI_PRI_COUNT] = {
    "SWI_PRI_0",
    "SWI_PRI_1",
    "SWI_PRI_2",
};


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * swiTaskFxn
 * @brief function that each SWI task sits in while waiting for a SWI to fire.
 *        fires the corresponding SWI based on which event bit is set
 * @param pvParameters void pointer to the priority, passed in by task creation
 */
static void swiTaskFxn(void* pvParameters)
{
    // convert void pointer to priority
    const RTOS_swiPri_E priority = (RTOS_swiPri_E)(uint32_t)pvParameters;

    // sit in this loop forever
    for (;;)
    {
        // wait forever for an event bit to get set. Yields back to scheduler
        EventBits_t events = RTOS_EVENT_ALL & xEventGroupWaitBits(rtos.swiEvents[priority], RTOS_EVENT_ALL, pdTRUE, pdFALSE, portMAX_DELAY);

        // once event bit has set, fire all SWIs that have their bits set
        while (events != 0U)
        {
            // find index of first event bit that is set
            uint32_t index = 31UL - u32CountLeadingZeroes(events);
            // clear that bit
            events &= ~(1UL << index);

            // get the corresponding SWI handle from the table at this priority
            const volatile RTOS_swiHandle_T* swi = &rtos.swiTable[priority][index];
            if (*swi->handler != NULL)
            {
                // call the SWI handler
                (*swi->handler)();
            }
        }
    }
}


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * RTOS_SWI_Init
 * @brief Initialize the SWI module
 */
void RTOS_SWI_Init(void)
{
    memset(&rtos, 0x00, sizeof(rtos));

    // create the event groups for each priority
    static StaticEventGroup_t eventGroups[RTOS_SWI_PRI_COUNT];

    // intialize the task for each SWI priority
    for (uint32_t pri = 0UL; pri < (uint32_t)RTOS_SWI_PRI_COUNT; pri++)
    {
        // create event group for this priority
        rtos.swiEvents[pri] = xEventGroupCreateStatic(&eventGroups[0]);

        StaticTask_t* swiTask;
        StackType_t * swiTaskStack;
        uint32_t      swiTaskStackSize;
        // allocate memory for this task
        RTOS_getSwiTaskmemory((RTOS_swiPri_E)pri, &swiTask, &swiTaskStack, &swiTaskStackSize);

        // create the task
        rtos.swiTasks[pri].handle = xTaskCreateStatic(&swiTaskFxn,               // handler function
                                                      taskNames[pri],            // task name
                                                      swiTaskStackSize,
                                                      (void*)pri,                // void pointer to task priority (passed as argument to the handler function)
                                                      RTOS_SWI_PRI_OFFSET + pri, // task priority
                                                      swiTaskStack,
                                                      swiTask);
    }
}

/**
 * SWI_create
 * @brief create a SWI with a given priority and handler function
 * @param priority priority of this SWI
 * @param handler handler function for this SWI
 * @return pointer to the created handler
 */
RTOS_swiHandle_T* SWI_create(RTOS_swiPri_E priority, RTOS_swiFn_t handler)
{
    // confirm that there is space in this priority level for another SWI
    if (rtos.swiCountPerPri[priority] >= RTOS_SWI_MAX_PER_PRI)
    {
        // no more space in the table for this priority
        return NULL;
    }
    else
    {
        uint8_t           index = rtos.swiCountPerPri[priority]++; // index in the table where this SWI will be placed
        RTOS_swiHandle_T* swi   = &rtos.swiTable[priority][index]; // handle entry from the SWI table

        // create the SWI
        swi->handler  = handler;                // link to SWI handler
        swi->priority = priority;               // link SWI priority
        swi->event    = 1UL << (uint32_t)index; // set event bit mask

        return swi;
    }
}

/**
 * SWI_invoke
 * @brief invoke the given SWI
 * @param handle SWI handle to invoke
 */
void SWI_invoke(RTOS_swiHandle_T *handle)
{
    // set the event bit for the given SWI
    (void)xEventGroupSetBits(rtos.swiEvents[handle->priority], handle->event);
}

/**
 * SWI_invokeFromISR
 * @brief invoke the given SWI from ISR context
 *        used to invoke a SWI when already running from an interrupt handler
 * @param handle SWI handle to invoke
 * @return return whether the message was successfully sent to the daemon
 */
bool SWI_invokeFromISR(RTOS_swiHandle_T *handle)
{
    // whether the daemon task (which is used to set the bit, since this is called from an ISR) was woken in order to set the bit
    // (i.e. the daemon task is a higher priority than the interrupt which is invoking the SWI)
    BaseType_t higherPrioTaskWoken = pdFALSE;
    BaseType_t result              = xEventGroupSetBitsFromISR(rtos.swiEvents[handle->priority],
                                                               handle->event,
                                                               &higherPrioTaskWoken);

    // required in order to switch contexts when the ISR returns
    portYIELD_FROM_ISR(higherPrioTaskWoken);

    return(result == pdPASS);
}

/**
 * SWI_disable
 * @brief enter critical section, don't let SWIs interrupt
 */
void SWI_disable(void)
{
    taskENTER_CRITICAL();
}

/**
 * SWI_enable
 * @brief exit critical section, allow SWIs to interrupt again
 */
void SWI_enable(void)
{
    taskEXIT_CRITICAL();
}
