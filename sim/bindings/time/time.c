#include "rig_runtime.h"

#include "runtime_state.h"

#include "HW_tim.h"

void rig_runtime_advance_time_ns(uint64_t elapsed_ns)
{
    rig_runtime.time_ns += elapsed_ns;
}

uint32_t HW_TIM_getTimeMS(void)
{
    return (uint32_t)(rig_runtime.time_ns / 1000000ULL);
}

uint32_t HW_getTick(void)
{
    return HW_TIM_getTimeMS();
}

uint64_t rig_runtime_get_time_ns(void)
{
    return rig_runtime.time_ns;
}

uint64_t HW_TIM_getBaseTick(void)
{
    return rig_runtime.time_ns / 1000ULL;
}

void HW_TIM_setDuty(HW_TIM_port_E port, HW_TIM_channel_E channel, float32_t percentage)
{
    const rig_timer_channel_event_S event = {
        .port         = (int32_t)port,
        .channel      = (int32_t)channel,
        .value        = percentage,
        .timestamp_ns = rig_runtime.time_ns,
    };

    (void)rig_runtime_timer_push_duty_output(&event);
}

void HW_TIM_setFreqHz(HW_TIM_port_E port, HW_TIM_channel_E channel, float32_t hz)
{
    const rig_timer_channel_event_S event = {
        .port         = (int32_t)port,
        .channel      = (int32_t)channel,
        .value        = hz,
        .timestamp_ns = rig_runtime.time_ns,
    };

    (void)rig_runtime_timer_push_frequency_output(&event);
}

float32_t HW_TIM_getDuty(HW_TIM_port_E port, HW_TIM_channel_E channel)
{
    float32_t value = 0.0f;

    (void)rig_runtime_timer_latest_duty_input((int32_t)port, (int32_t)channel, &value);
    return value;
}

float32_t HW_TIM_getFreqHz(HW_TIM_port_E port, HW_TIM_channel_E channel)
{
    float32_t value = 0.0f;

    (void)rig_runtime_timer_latest_frequency_input((int32_t)port, (int32_t)channel, &value);
    return value;
}
