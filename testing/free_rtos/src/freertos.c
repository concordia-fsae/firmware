/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "main.h"
#include "task.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t       osStaticThreadDef_t;
typedef StaticQueue_t      osStaticMessageQDef_t;
typedef StaticTimer_t      osStaticTimerDef_t;
typedef StaticSemaphore_t  osStaticMutexDef_t;
typedef StaticSemaphore_t  osStaticSemaphoreDef_t;
typedef StaticEventGroup_t osStaticEventGroupDef_t;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t         defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name       = "defaultTask",
    .stack_size = 128 * 4,
    .priority   = (osPriority_t)osPriorityNormal,
};
/* Definitions for myTask02 */
osThreadId_t         myTask02Handle;
uint32_t             myTask02Buffer[128];
osStaticThreadDef_t  myTask02ControlBlock;
const osThreadAttr_t myTask02_attributes = {
    .name       = "myTask02",
    .cb_mem     = &myTask02ControlBlock,
    .cb_size    = sizeof(myTask02ControlBlock),
    .stack_mem  = &myTask02Buffer[0],
    .stack_size = sizeof(myTask02Buffer),
    .priority   = (osPriority_t)osPriorityLow,
};
/* Definitions for myQueue01 */
osMessageQueueId_t         myQueue01Handle;
const osMessageQueueAttr_t myQueue01_attributes = {
    .name = "myQueue01"
};
/* Definitions for myQueue02 */
osMessageQueueId_t         myQueue02Handle;
uint8_t                    myQueue02Buffer[16 * sizeof(uint16_t)];
osStaticMessageQDef_t      myQueue02ControlBlock;
const osMessageQueueAttr_t myQueue02_attributes = {
    .name    = "myQueue02",
    .cb_mem  = &myQueue02ControlBlock,
    .cb_size = sizeof(myQueue02ControlBlock),
    .mq_mem  = &myQueue02Buffer,
    .mq_size = sizeof(myQueue02Buffer)
};
/* Definitions for myTimer01 */
osTimerId_t         myTimer01Handle;
const osTimerAttr_t myTimer01_attributes = {
    .name = "myTimer01"
};
/* Definitions for myTimer02 */
osTimerId_t         myTimer02Handle;
osStaticTimerDef_t  myTimer02ControlBlock;
const osTimerAttr_t myTimer02_attributes = {
    .name    = "myTimer02",
    .cb_mem  = &myTimer02ControlBlock,
    .cb_size = sizeof(myTimer02ControlBlock),
};
/* Definitions for myMutex01 */
osMutexId_t         myMutex01Handle;
const osMutexAttr_t myMutex01_attributes = {
    .name = "myMutex01"
};
/* Definitions for myMutex02 */
osMutexId_t         myMutex02Handle;
osStaticMutexDef_t  myMutex02ControlBlock;
const osMutexAttr_t myMutex02_attributes = {
    .name    = "myMutex02",
    .cb_mem  = &myMutex02ControlBlock,
    .cb_size = sizeof(myMutex02ControlBlock),
};
/* Definitions for myRecursiveMutex01 */
osMutexId_t         myRecursiveMutex01Handle;
const osMutexAttr_t myRecursiveMutex01_attributes = {
    .name      = "myRecursiveMutex01",
    .attr_bits = osMutexRecursive,
};
/* Definitions for myRecursiveMutex02 */
osMutexId_t         myRecursiveMutex02Handle;
osStaticMutexDef_t  myRecursiveMutex02ControlBlock;
const osMutexAttr_t myRecursiveMutex02_attributes = {
    .name      = "myRecursiveMutex02",
    .attr_bits = osMutexRecursive,
    .cb_mem    = &myRecursiveMutex02ControlBlock,
    .cb_size   = sizeof(myRecursiveMutex02ControlBlock),
};
/* Definitions for myBinarySem01 */
osSemaphoreId_t         myBinarySem01Handle;
const osSemaphoreAttr_t myBinarySem01_attributes = {
    .name = "myBinarySem01"
};
/* Definitions for myBinarySem02 */
osSemaphoreId_t         myBinarySem02Handle;
osStaticSemaphoreDef_t  myBinarySem02ControlBlock;
const osSemaphoreAttr_t myBinarySem02_attributes = {
    .name    = "myBinarySem02",
    .cb_mem  = &myBinarySem02ControlBlock,
    .cb_size = sizeof(myBinarySem02ControlBlock),
};
/* Definitions for myCountingSem01 */
osSemaphoreId_t         myCountingSem01Handle;
const osSemaphoreAttr_t myCountingSem01_attributes = {
    .name = "myCountingSem01"
};
/* Definitions for myCountingSem02 */
osSemaphoreId_t         myCountingSem02Handle;
osStaticSemaphoreDef_t  myCountingSem02ControlBlock;
const osSemaphoreAttr_t myCountingSem02_attributes = {
    .name    = "myCountingSem02",
    .cb_mem  = &myCountingSem02ControlBlock,
    .cb_size = sizeof(myCountingSem02ControlBlock),
};
/* Definitions for myEvent01 */
osEventFlagsId_t         myEvent01Handle;
const osEventFlagsAttr_t myEvent01_attributes = {
    .name = "myEvent01"
};
/* Definitions for myEvent02 */
osEventFlagsId_t         myEvent02Handle;
osStaticEventGroupDef_t  myEvent02ControlBlock;
const osEventFlagsAttr_t myEvent02_attributes = {
    .name    = "myEvent02",
    .cb_mem  = &myEvent02ControlBlock,
    .cb_size = sizeof(myEvent02ControlBlock),
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void* argument);
void StartTask02(void* argument);
void Callback01(void* argument);
void Callback02(void* argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationIdleHook(void);

/* USER CODE BEGIN 2 */
void vApplicationIdleHook(void)
{
    /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
    to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
    task. It is essential that code added to this hook function never attempts
    to block in any way (for example, call xQueueReceive() with a block time
    specified, or call vTaskDelay()). If the application makes use of the
    vTaskDelete() API function (as this demo application does) then it is also
    important that vApplicationIdleHook() is permitted to return to its calling
    function, because it is the responsibility of the idle task to clean up
    memory allocated by the kernel to any task that has since been deleted. */
}
/* USER CODE END 2 */

/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void MX_FREERTOS_Init(void)
{
    /* USER CODE BEGIN Init */

    /* USER CODE END Init */
    /* Create the mutex(es) */
    /* creation of myMutex01 */
    myMutex01Handle = osMutexNew(&myMutex01_attributes);

    /* creation of myMutex02 */
    myMutex02Handle = osMutexNew(&myMutex02_attributes);

    /* Create the recursive mutex(es) */
    /* creation of myRecursiveMutex01 */
    myRecursiveMutex01Handle = osMutexNew(&myRecursiveMutex01_attributes);

    /* creation of myRecursiveMutex02 */
    myRecursiveMutex02Handle = osMutexNew(&myRecursiveMutex02_attributes);

    /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
    /* USER CODE END RTOS_MUTEX */

    /* Create the semaphores(s) */
    /* creation of myBinarySem01 */
    myBinarySem01Handle = osSemaphoreNew(1, 1, &myBinarySem01_attributes);

    /* creation of myBinarySem02 */
    myBinarySem02Handle = osSemaphoreNew(1, 1, &myBinarySem02_attributes);

    /* creation of myCountingSem01 */
    myCountingSem01Handle = osSemaphoreNew(2, 2, &myCountingSem01_attributes);

    /* creation of myCountingSem02 */
    myCountingSem02Handle = osSemaphoreNew(2, 2, &myCountingSem02_attributes);

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
    /* USER CODE END RTOS_SEMAPHORES */

    /* Create the timer(s) */
    /* creation of myTimer01 */
    myTimer01Handle = osTimerNew(Callback01, osTimerPeriodic, NULL, &myTimer01_attributes);

    /* creation of myTimer02 */
    myTimer02Handle = osTimerNew(Callback02, osTimerPeriodic, NULL, &myTimer02_attributes);

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    /* USER CODE END RTOS_TIMERS */

    /* Create the queue(s) */
    /* creation of myQueue01 */
    myQueue01Handle = osMessageQueueNew(16, sizeof(uint16_t), &myQueue01_attributes);

    /* creation of myQueue02 */
    myQueue02Handle = osMessageQueueNew(16, sizeof(uint16_t), &myQueue02_attributes);

    /* USER CODE BEGIN RTOS_QUEUES */
    /* add queues, ... */
    /* USER CODE END RTOS_QUEUES */

    /* Create the thread(s) */
    /* creation of defaultTask */
    defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

    /* creation of myTask02 */
    myTask02Handle = osThreadNew(StartTask02, NULL, &myTask02_attributes);

    /* USER CODE BEGIN RTOS_THREADS */
    /* add threads, ... */
    /* USER CODE END RTOS_THREADS */

    /* creation of myEvent01 */
    myEvent01Handle = osEventFlagsNew(&myEvent01_attributes);

    /* creation of myEvent02 */
    myEvent02Handle = osEventFlagsNew(&myEvent02_attributes);

    /* USER CODE BEGIN RTOS_EVENTS */
    /* add events, ... */
    /* USER CODE END RTOS_EVENTS */
}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void* argument)
{
    /* USER CODE BEGIN StartDefaultTask */
    /* Infinite loop */
    for (;;)
    {
        osDelay(1);
    }
    /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTask02 */
/**
 * @brief Function implementing the myTask02 thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTask02 */
void StartTask02(void* argument)
{
    /* USER CODE BEGIN StartTask02 */
    /* Infinite loop */
    for (;;)
    {
        osDelay(1);
    }
    /* USER CODE END StartTask02 */
}

/* Callback01 function */
void Callback01(void* argument)
{
    /* USER CODE BEGIN Callback01 */

    /* USER CODE END Callback01 */
}

/* Callback02 function */
void Callback02(void* argument)
{
    /* USER CODE BEGIN Callback02 */

    /* USER CODE END Callback02 */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
