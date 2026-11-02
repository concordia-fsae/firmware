/**
 * @brief Deterministic, single-threaded FreeRTOS test environment.
 *
 * Time uses one millisecond per tick. Queues use caller-owned static storage;
 * reset clears controls/hooks, and fixtures must recreate their queues.
 * Blocking peek delegates to on_block (which may longjmp out of a worker).
 * Send/receive support nonblocking calls only. No task concurrency is modeled.
 */
#pragma once
#include "FreeRTOS.h"

typedef struct
{
    uint32_t ticks;
    int      scheduler_state;
    unsigned critical_depth;
    bool     reject_enqueue;
    void (*on_block)(QueueHandle_t queue);
    void (*on_delay)(void);
} test_freertos_S;

extern test_freertos_S test_freertos;
void test_freertos_reset(void);
