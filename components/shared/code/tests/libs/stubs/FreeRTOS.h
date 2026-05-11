#pragma once

#include <stdbool.h>
#include <stdint.h>

#define taskSCHEDULER_RUNNING 1
#define errQUEUE_FULL 0
#define pdFALSE 0
#define pdTRUE 1
#define portMAX_DELAY 0xFFFFFFFFUL
#define pdMS_TO_TICKS(ms) (ms)

typedef void* QueueHandle_t;
typedef struct
{
    uint8_t unused;
} StaticQueue_t;

static inline QueueHandle_t xQueueCreateStatic(uint32_t queue_length,
                                               uint32_t item_size,
                                               uint8_t* queue_storage,
                                               StaticQueue_t* queue_buffer)
{
    (void)queue_length;
    (void)item_size;
    (void)queue_storage;
    return (QueueHandle_t)queue_buffer;
}

static inline int xQueuePeek(QueueHandle_t queue, void* item, uint32_t ticks_to_wait)
{
    (void)queue;
    (void)item;
    (void)ticks_to_wait;
    return pdFALSE;
}

static inline int xQueueReceive(QueueHandle_t queue, void* item, uint32_t ticks_to_wait)
{
    (void)queue;
    (void)item;
    (void)ticks_to_wait;
    return pdFALSE;
}

static inline uint32_t uxQueueMessagesWaiting(QueueHandle_t queue)
{
    (void)queue;
    return 0U;
}

static inline int xQueueSend(QueueHandle_t queue, const void* item, uint32_t ticks_to_wait)
{
    (void)queue;
    (void)item;
    (void)ticks_to_wait;
    return pdTRUE;
}

static inline int xTaskGetSchedulerState(void)
{
    return 0;
}

static inline void vTaskDelay(uint32_t ticks)
{
    (void)ticks;
}

#define taskENTER_CRITICAL() \
    do \
    { \
    } while (0)

#define taskEXIT_CRITICAL() \
    do \
    { \
    } while (0)

