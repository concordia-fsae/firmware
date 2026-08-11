#pragma once

#include <stdint.h>

#define pdMS_TO_TICKS(ms)        (ms)
#define portMAX_DELAY            UINT32_MAX
#define taskSCHEDULER_RUNNING    1
#define errQUEUE_FULL            0

typedef void* QueueHandle_t;

typedef struct
{
    uint32_t opaque;
} StaticQueue_t;

extern uint32_t    HW_TIM_getTimeMS(void);

static inline void vTaskStartScheduler(void)
{
}

static inline void vTaskDelay(uint32_t ticks)
{
    (void)ticks;
}

static inline uint32_t xTaskGetTickCount(void)
{
    return HW_TIM_getTimeMS();
}

static inline TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    return (TaskHandle_t)0;
}

static inline uint32_t ulTaskGetRunTimePercent(TaskHandle_t task)
{
    (void)task;
    return 0;
}

static inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task)
{
    (void)task;
    return 0;
}

static inline int xTaskGetSchedulerState(void)
{
    return taskSCHEDULER_RUNNING;
}

static inline uint32_t ulTaskNotifyTake(uint32_t clear_on_exit, uint32_t ticks_to_wait)
{
    (void)clear_on_exit;
    (void)ticks_to_wait;
    return 0;
}

static inline int xTaskNotifyGive(TaskHandle_t task)
{
    (void)task;
    return pdTRUE;
}

static inline void taskENTER_CRITICAL(void)
{
}

static inline void taskEXIT_CRITICAL(void)
{
}

static inline QueueHandle_t xQueueCreateStatic(
    UBaseType_t  queue_length,
    UBaseType_t  item_size,
    uint8_t      * storage,
    StaticQueue_t* queue)
{
    (void)queue_length;
    (void)item_size;
    (void)storage;
    return queue;
}

static inline int xQueuePeek(QueueHandle_t queue, void* item, uint32_t ticks)
{
    (void)queue;
    (void)item;
    (void)ticks;
    return pdFALSE;
}

static inline int xQueueReceive(QueueHandle_t queue, void* item, uint32_t ticks)
{
    (void)queue;
    (void)item;
    (void)ticks;
    return pdFALSE;
}

static inline int xQueueSend(QueueHandle_t queue, const void* item, uint32_t ticks)
{
    (void)queue;
    (void)item;
    (void)ticks;
    return pdTRUE;
}

static inline UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue)
{
    (void)queue;
    return 0;
}
