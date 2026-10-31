#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint8_t  *buffer;
    unsigned capacity;
    unsigned item_size;
    unsigned count;
} StaticQueue_t;
typedef StaticQueue_t *QueueHandle_t;
#define portMAX_DELAY            UINT32_MAX
#define taskSCHEDULER_RUNNING    1
#define errQUEUE_FULL            0
#define pdMS_TO_TICKS(ms)        (ms)
#define taskENTER_CRITICAL()     test_enter_critical()
#define taskEXIT_CRITICAL()      test_exit_critical()

QueueHandle_t xQueueCreateStatic(unsigned capacity, unsigned item_size, uint8_t *buffer, StaticQueue_t *queue);
int           xQueueSend(QueueHandle_t queue, const void *item, uint32_t timeout);
int           xQueuePeek(QueueHandle_t queue, void *item, uint32_t timeout);
int           xQueueReceive(QueueHandle_t queue, void *item, uint32_t timeout);
unsigned      uxQueueMessagesWaiting(QueueHandle_t queue);
int           xTaskGetSchedulerState(void);
void          vTaskDelay(uint32_t ticks);
void          test_enter_critical(void);
void          test_exit_critical(void);
