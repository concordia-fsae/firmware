#pragma once

#include <stdint.h>

extern uint32_t shared_code_test_hw_time_ms;

static inline uint32_t HW_TIM_getTimeMS(void)
{
    return shared_code_test_hw_time_ms;
}

static inline void shared_code_test_hw_setTimeMS(uint32_t time_ms)
{
    shared_code_test_hw_time_ms = time_ms;
}

