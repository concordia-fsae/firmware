/**
 * @file FreeRTOSResources.c
 * @brief  Source code for FreeRTOS Implementation
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version
 * @date 2024-01-30
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "FreeRTOS_types.h"
#include "SystemConfig.h"
#include <stdlib.h>

// FreeRTOS includes
#include "FreeRTOS.h"
#include "FreeRTOS_SWI.h"
#include "task.h"
#include "timers.h"

// Other Includes
#include "CAN/CAN.h"
#include "Utility.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

// which bit in the event group corresponds to each task
#define PERIODIC_TASK_1kHz  (1U) << (0U)
#define PERIODIC_TASK_100Hz (1U) << (1U)
#define PERIODIC_TASK_10Hz  (1U) << (2U)
#define PERIODIC_TASK_1Hz   (1U) << (3U)


/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

#define NUM_TASKS (sizeof(ModuleTasks) / sizeof(RTOS_taskDesc_t))    // number of tasks in this module


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

// periodic event group. each bit in the group signifies that the
// respective periodic task is ready to run
EventGroupHandle_t PeriodicEvent;

// timer which will run all of the periodic tasks
TimerHandle_t rtos_tick_timer;

// task handle and stack definitions
static StaticTask_t Task1kHz;
static StackType_t  task1kHzStack[configMINIMAL_STACK_SIZE];
static StaticTask_t Task100Hz;
static StackType_t  task100HzStack[configMINIMAL_STACK_SIZE];
static StaticTask_t Task10Hz;
static StackType_t  task10HzStack[configMINIMAL_STACK_SIZE];
static StaticTask_t Task1Hz;
static StackType_t  task1HzStack[configMINIMAL_STACK_SIZE];
#if FEATURE_IS_ENABLED(NVM_TASK)
static StaticTask_t TaskNvm;
static StackType_t  taskNvmStack[configMINIMAL_STACK_SIZE];
#endif

// module periodic tasks, defined in Module.h
extern void Module_1kHz_TSK(void);
extern void Module_100Hz_TSK(void);
extern void Module_10Hz_TSK(void);
extern void Module_1Hz_TSK(void);
#if FEATURE_IS_ENABLED(NVM_TASK)
extern void lib_nvm_run(void);
#endif
extern void Module_ApplicationIdleHook(void);

// SWIs
RTOS_swiHandle_T* CANRX_swi;
RTOS_swiHandle_T* CANTX_swi;

// task definitions
RTOS_taskDesc_t ModuleTasks[] = {
    {
        .function    = &Module_1kHz_TSK,
        .name        = "Task 1kHz",
        .priority    = 7U,
        .parameters  = NULL,
        .stack       = task1kHzStack,
        .stackSize   = sizeof(task1kHzStack) / sizeof(StackType_t),
        .stateBuffer = &Task1kHz,
        .event       = {
                  .group = &PeriodicEvent,
                  .bit   = PERIODIC_TASK_1kHz,
        },
        .periodMs = pdMS_TO_TICKS(1U),
    },
    {
        .function    = &Module_100Hz_TSK,
        .name        = "Task 100Hz",
        .priority    = 6U,
        .parameters  = NULL,
        .stack       = task100HzStack,
        .stackSize   = sizeof(task100HzStack) / sizeof(StackType_t),
        .stateBuffer = &Task100Hz,
        .event       = {
                  .group = &PeriodicEvent,
                  .bit   = PERIODIC_TASK_100Hz,
        },
        .periodMs = pdMS_TO_TICKS(10U),
    },
    {
        .function    = &Module_10Hz_TSK,
        .name        = "Task 10Hz",
        .priority    = 5U,
        .stack       = task10HzStack,
        .stackSize   = sizeof(task10HzStack) / sizeof(StackType_t),
        .stateBuffer = &Task10Hz,
        .parameters  = NULL,
        .event       = {
                  .group = &PeriodicEvent,
                  .bit   = PERIODIC_TASK_10Hz,
        },
        .periodMs = pdMS_TO_TICKS(100U),
    },
    {
        .function    = &Module_1Hz_TSK,
        .name        = "Task 1Hz",
        .priority    = 4U,
        .stack       = task1HzStack,
        .stackSize   = sizeof(task1HzStack) / sizeof(StackType_t),
        .stateBuffer = &Task1Hz,
        .parameters  = NULL,
        .event       = {
                  .group = &PeriodicEvent,
                  .bit   = PERIODIC_TASK_1Hz,
        },
        .periodMs = pdMS_TO_TICKS(1000U),
    },
};
RTOS_taskDesc_t FreerunTasks[] = {
#if FEATURE_IS_ENABLED(NVM_TASK)
    {
        .function    = &lib_nvm_run,
        .name        = "NVM Task",
        .priority    = 8U,
        .stack       = taskNvmStack,
        .stackSize   = sizeof(taskNvmStack) / sizeof(StackType_t),
        .stateBuffer = &TaskNvm,
        .parameters  = NULL,
    },
#endif
};


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void vApplicationIdleHook(void);
void vApplicationTickHook(void);


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void rtosTickTimer(TimerHandle_t xTimer)
{
    if (xTimer == rtos_tick_timer)
    {
        for (uint16_t i = 0U; i < NUM_TASKS; i++)
        {
            RTOS_taskDesc_t* const task = &ModuleTasks[i];

            if (task->periodMs == 0U)
            {
                // aperiodic task
                continue;
            }

            // check if it is time to execute the task
            if (++task->timeSinceLastTickMs == task->periodMs)
            {
                task->timeSinceLastTickMs = 0U;
                // if it is, set the event bit for this task in the event group
                xEventGroupSetBits(*task->event.group, task->event.bit);
            }
        }
    }
}

/*
 * Task Function
 * This function is called by FreeRTOS for each task. Each task will then,
 * in turn, check to see if it is time to execute based on its period and
 * time since last execution. If it's time to execute, this function will call
 * the function pointer associated with the task
 */
static void taskFxn(void* parameters)
{
    RTOS_taskDesc_t* task = (RTOS_taskDesc_t*)parameters;    // convert the parameter back into a task description

    void (*const function)(void)    = task->function;       // get the function that will be called for this task
    EventGroupHandle_t* event_group = task->event.group;    // get the shared event group
    EventBits_t         event_bit   = task->event.bit;      // get the event bit for this task

    // if the processor has a floating point unit, uncomment the following line
    // portTASK_USES_FLOATING_POINT();

    for (;;)
    {
        // check if the event bit for this task has been set. If it has, call the function
        // for this task. If not, non-blocking wait.
        xEventGroupWaitBits(*event_group, event_bit, pdTRUE, pdFALSE, portMAX_DELAY);

        (*function)();
    }
}

/*
 * Freerun Task Function
 * This function is called by FreeRTOS for each task. Each task will then,
 * in turn, check to see if it is time to execute based on its period and
 * time since last execution. If it's time to execute, this function will call
 * the function pointer associated with the task
 */
static void freerunTaskFxn(void* parameters)
{
    RTOS_taskDesc_t* task = (RTOS_taskDesc_t*)parameters;    // convert the parameter back into a task description

    void (*const function)(void)    = task->function;       // get the function that will be called for this task

    // if the processor has a floating point unit, uncomment the following line
    // portTASK_USES_FLOATING_POINT();

    (*function)();
}

/**
 * vApplicationIdleHook
 * @brief function that runs when the scheduler is idle
 */
void vApplicationIdleHook(void)
{
    /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
     * to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
     * task. It is essential that code added to this hook function never attempts
     * to block in any way (for example, call xQueueReceive() with a block time
     * specified, or call vTaskDelay()). If the application makes use of the
     * vTaskDelete() API function (as this demo application does) then it is also
     * important that vApplicationIdleHook() is permitted to return to its calling
     * function, because it is the responsibility of the idle task to clean up
     * memory allocated by the kernel to any task that has since been deleted. */
    Module_ApplicationIdleHook();
}

void vApplicationTickHook(void)
{
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * RTOS_createResources
 * @brief create resources that are required by FreeRTOS
 *        namely, all of the tasks, timers, and SWIs
 */
void RTOS_createResources(void);
void RTOS_createResources(void)
{
    /*
     * create SWI handles
     */
#if FEATURE_IS_ENABLED(FEATURE_CANRX_SWI)
    CANRX_swi = SWI_create(RTOS_SWI_PRI_0, &CANRX_SWI);
#endif // FEATURE_CANRX_SWI
#if FEATURE_IS_ENABLED(FEATURE_CANTX_SWI)
    CANTX_swi = SWI_create(RTOS_SWI_PRI_0, &CANTX_SWI);
#endif // FEATURE_CANTX_SWI

    /*
     * Create tasks
     */
    for (uint16_t i = 0U; i < NUM_TASKS; i++)
    {
        RTOS_taskDesc_t* const task = &ModuleTasks[i];

        task->handle = xTaskCreateStatic(&taskFxn,
                                         task->name,
                                         task->stackSize,
                                         task,
                                         task->priority,
                                         task->stack,
                                         task->stateBuffer);
    }
    for (uint16_t i = 0U; i < COUNTOF(FreerunTasks); i++)
    {
        RTOS_taskDesc_t* const task = &FreerunTasks[i];
        task->handle = xTaskCreateStatic(&freerunTaskFxn,
                                            task->name,
                                            task->stackSize,
                                            task,
                                            task->priority,
                                            task->stack,
                                            task->stateBuffer);
    }

    /*
     * Create timers
     */
    static StaticTimer_t rtosTickTimerState;

    // 1kHz timer drives all tasks
    // TODO: should this be faster and/or interrupt (i.e. hardware timer) driven?
    rtos_tick_timer = xTimerCreateStatic("Timer 10kHz",
                                         pdMS_TO_TICKS(1U),
                                         pdTRUE,
                                         NULL,
                                         &rtosTickTimer,
                                         &rtosTickTimerState);

    // start the timer (it will only start once the scheduler starts)
    (void)xTimerStart(rtos_tick_timer, 0U);

    /*
     * Create event groups
     */
    static StaticEventGroup_t PeriodicTickEventGroup;

    PeriodicEvent = xEventGroupCreateStatic(&PeriodicTickEventGroup);
}


/******************************************************************************
 *                              O S  H O O K S
 ******************************************************************************/
extern void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer,
                                          StackType_t**  ppxIdleTaskStackBuffer,
                                          uint32_t*      pusIdleTaskStackSize);

/**
 * vApplicationGetIdleTaskMemory
 * @brief allocate memory for the idle task
 * @param ppxIdleTaskTCBBuffer TODO
 * @param ppxIdleTaskStackBuffer TODO
 * @param pusIdleTaskStackSize TODO
 */
void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer,
                                   StackType_t**  ppxIdleTaskStackBuffer,
                                   uint32_t*      pusIdleTaskStackSize)
{
    static StaticTask_t idleTask;
    static StackType_t  idleTaskStack[configIDLE_TASK_STACK_DEPTH];

    *ppxIdleTaskTCBBuffer   = &idleTask;
    *ppxIdleTaskStackBuffer = idleTaskStack;
    *pusIdleTaskStackSize   = sizeof(idleTaskStack) / sizeof(StackType_t);
}

extern void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer,
                                           StackType_t**  ppxTimerTaskStackBuffer,
                                           uint32_t*      pusTimerTaskStackSize);


/**
 * vApplicationGetTimerTaskMemory
 * @brief allocate memory for the timer task
 * @param ppxTimerTaskTCBBuffer TODO
 * @param ppxTimerTaskStackBuffer TODO
 * @param pusTimerTaskStackSize TODO
 */
void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer,
                                    StackType_t**  ppxTimerTaskStackBuffer,
                                    uint32_t*      pusTimerTaskStackSize)
{
    static StaticTask_t timerTask;
    static StackType_t  timerTaskStack[configTIMER_TASK_STACK_DEPTH];

    *ppxTimerTaskTCBBuffer   = &timerTask;
    *ppxTimerTaskStackBuffer = timerTaskStack;
    *pusTimerTaskStackSize   = sizeof(timerTaskStack) / sizeof(StackType_t);
}


/**
 * RTOS_getSwiTaskMemory
 * @brief allocate memory for the SWI tasks
 * @param swiPriority priority of the SWI task this is being called for
 * @param ppxSwiTaskTCBBuffer pointer to resultant task memory for this SWI priority
 * @param ppxSwiTaskStackBuffer pointer to the resultant stack for this SWI priority
 * @param pusSwiTaskStackSize pointer to the stack size var for this SWI priority
 */
void RTOS_getSwiTaskMemory(RTOS_swiPri_E  swiPriority,
                           StaticTask_t** ppxSwiTaskTCBBuffer,
                           StackType_t**  ppxSwiTaskStackBuffer,
                           uint32_t*      pusSwiTaskStackSize)
{
    switch (swiPriority)
    {
        case RTOS_SWI_PRI_0:
        {
            // create task memory and stack
            static StaticTask_t swiPri0Task;
            static StackType_t  swiPri0TaskStack[configMINIMAL_STACK_SIZE] __attribute__((aligned(portBYTE_ALIGNMENT)));
            // link to relevant pointers
            *ppxSwiTaskTCBBuffer   = &swiPri0Task;
            *ppxSwiTaskStackBuffer = swiPri0TaskStack;
            *pusSwiTaskStackSize   = COUNTOF(swiPri0TaskStack);
            break;
        }

        case RTOS_SWI_PRI_1:
        {
            static StaticTask_t swiPri1Task;
            static StackType_t  swiPri1TaskStack[configMINIMAL_STACK_SIZE] __attribute__((aligned(portBYTE_ALIGNMENT)));
            *ppxSwiTaskTCBBuffer   = &swiPri1Task;
            *ppxSwiTaskStackBuffer = swiPri1TaskStack;
            *pusSwiTaskStackSize   = COUNTOF(swiPri1TaskStack);
            break;
        }

        case RTOS_SWI_PRI_2:
        {
            static StaticTask_t swiPri2Task;
            static StackType_t  swiPri2TaskStack[configMINIMAL_STACK_SIZE] __attribute__((aligned(portBYTE_ALIGNMENT)));
            *ppxSwiTaskTCBBuffer   = &swiPri2Task;
            *ppxSwiTaskStackBuffer = swiPri2TaskStack;
            *pusSwiTaskStackSize   = COUNTOF(swiPri2TaskStack);
            break;
        }

        default:
            // should never get here
            *ppxSwiTaskTCBBuffer   = NULL;
            *ppxSwiTaskStackBuffer = NULL;
            *pusSwiTaskStackSize   = 0UL;
            break;
    }
}
