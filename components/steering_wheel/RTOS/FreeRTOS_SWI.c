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

typedef struct
{
    RTOS_swiHandle_T swiTable[RTOS_SWI_PRI_COUNT][RTOS_SWI_MAX_PER_PRI];
    RTOS_taskDesc_t swiTasks[RTOS_SWI_PRI_COUNT];
    EventGroupHandle_t swiEvents[RTOS_SWI_PRI_COUNT];
    uint8_t swiCountPerPri[RTOS_SWI_PRI_COUNT];
} rtos_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static rtos_S rtos;
static const char* taskNames[RTOS_SWI_PRI_COUNT] = {
    "SWI_PRI_0",
    "SWI_PRI_1",
    "SWI_PRI_2",
};


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void swiTaskFn(void* pvParameters)
{
    const RTOS_swiPri_E priority = (RTOS_swiPri_E)(uint32_t)pvParameters;

    for(;;)
    {
        EventBits_t events = RTOS_EVENT_ALL & xEventGroupWaitBits(rtos.swiEvents[priority], RTOS_EVENT_ALL, pdTRUE, pdFALSE, portMAX_DELAY);

        while (events != 0U)
        {
            uint32_t index = 31UL - u32CountLeadingZeroes(events);
            events &= ~(1UL << index);

            const volatile RTOS_swiHandle_T* swi = &rtos.swiTable[priority][index];
            if (*swi->handler != NULL)
            {
                (*swi->handler)();
            }
        }
    }
}


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void RTOS_SWI_Init(void)
{
    memset(&rtos, 0x00, sizeof(rtos));

    static StaticEventGroup_t swiEventGroups[RTOS_SWI_PRI_COUNT];
    for (uint32_t pri = 0UL; pri < (uint32_t)RTOS_SWI_PRI_COUNT; pri++)
    {
        rtos.swiEvents[pri] = xEventGroupCreateStatic(&swiEventGroups[0]);

        StaticTask_t* swiTask;
        StackType_t* swiTaskStack;
        uint32_t swiTaskStackSize;
        RTOS_getSwiTaskmemory((RTOS_swiPri_E) pri, &swiTask, &swiTaskStack, &swiTaskStackSize);

        rtos.swiTasks[pri].handle = xTaskCreateStatic(&swiTaskFn,
                                               taskNames[pri],
                                               swiTaskStackSize,
                                               (void*) pri,
                                               RTOS_SWI_PRI_OFFSET + pri,
                                               swiTaskStack,
                                               swiTask);
    }

}

RTOS_swiHandle_T* RTOS_swiCreate(RTOS_swiPri_E priority, RTOS_swiFn_t handler)
{
    if (rtos.swiCountPerPri[priority] >= RTOS_SWI_MAX_PER_PRI)
    {
        // no more space in the table for this priority
        return NULL;
    }
    else
    {
        uint8_t index = rtos.swiCountPerPri[priority]++;
        RTOS_swiHandle_T* swi = &rtos.swiTable[priority][index];

        // create the SWI
        swi->handler = handler;
        swi->priority = priority;
        swi->event = 1UL << (uint32_t) index;

        return swi;
    }
}

void RTOS_swiInvoke(RTOS_swiHandle_T *handle)
{
    (void)xEventGroupSetBits(rtos.swiEvents[handle->priority], handle->event);
}

bool RTOS_swiInvokeFromISR(RTOS_swiHandle_T *handle)
{
    BaseType_t higherPrioTaskWoken = pdFALSE;
    BaseType_t result = xEventGroupSetBitsFromISR(rtos.swiEvents[handle->priority], handle->event, &higherPrioTaskWoken);
    portYIELD_FROM_ISR(higherPrioTaskWoken);
    return (result == pdPASS);
}

void RTOS_swiDisable(void)
{
    taskENTER_CRITICAL();
}

void RTOS_swiEnable(void)
{
    taskEXIT_CRITICAL();
}
