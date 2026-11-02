/** @brief Shared deterministic queue, scheduler and clock stubs for C tests. */
#include "test_freertos.h"
#include "unity.h"
#include <string.h>

test_freertos_S test_freertos;

void test_freertos_reset(void)
{
    memset(&test_freertos, 0, sizeof(test_freertos));
}

int  xTaskGetSchedulerState(void) { return test_freertos.scheduler_state; }
void test_enter_critical(void)    { test_freertos.critical_depth++; }
void test_exit_critical(void)
{
    TEST_ASSERT_GREATER_THAN_UINT(0U, test_freertos.critical_depth);
    test_freertos.critical_depth--;
}

QueueHandle_t xQueueCreateStatic(unsigned capacity, unsigned item_size,
                                 uint8_t *buffer, StaticQueue_t *queue)
{
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(queue);
    TEST_ASSERT_GREATER_THAN_UINT(0U, capacity);
    TEST_ASSERT_GREATER_THAN_UINT(0U, item_size);
    *queue = (StaticQueue_t){ buffer, capacity, item_size, 0U };
    return queue;
}

int xQueueSend(QueueHandle_t queue, const void *item, uint32_t timeout)
{
    TEST_ASSERT_EQUAL_UINT32(0U, timeout);
    if (test_freertos.reject_enqueue || (queue->count == queue->capacity))
    {
        return errQUEUE_FULL;
    }
    memcpy(queue->buffer + queue->count++ * queue->item_size, item, queue->item_size);
    return 1;
}

int xQueuePeek(QueueHandle_t queue, void *item, uint32_t timeout)
{
    if ((queue->count == 0U) && (timeout != 0U))
    {
        TEST_ASSERT_EQUAL_UINT32(portMAX_DELAY, timeout);
        TEST_ASSERT_TRUE_MESSAGE(test_freertos.on_block != NULL,
                                 "Blocking queue peek requires a fixture hook");
        test_freertos.on_block(queue);
    }
    if (queue->count == 0U)
    {
        return 0;
    }
    memcpy(item, queue->buffer, queue->item_size);
    return 1;
}

int xQueueReceive(QueueHandle_t queue, void *item, uint32_t timeout)
{
    TEST_ASSERT_EQUAL_UINT32(0U, timeout);
    if (queue->count == 0U)
    {
        return 0;
    }
    memcpy(item, queue->buffer, queue->item_size);
    queue->count--;
    memmove(queue->buffer, queue->buffer + queue->item_size, queue->count * queue->item_size);
    return 1;
}

unsigned uxQueueMessagesWaiting(QueueHandle_t queue) { return queue->count; }

void     vTaskDelay(uint32_t ticks)
{
    test_freertos.ticks += ticks;
    if (test_freertos.on_delay != NULL)
    {
        test_freertos.on_delay();
    }
}
