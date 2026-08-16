#include "rig_runtime.h"

#include "HW_tim.h"

float32_t HW_TIM_getFreq(HW_TIM_channelFreq_E channel)
{
    float32_t value = 0.0f;

    (void)rig_runtime_timer_latest_capture_input((int32_t)channel, &value);
    return value;
}

uint64_t HW_TIM_getLastCaptureBaseTick(HW_TIM_channelFreq_E channel)
{
    (void)channel;
    return HW_TIM_getBaseTick();
}
