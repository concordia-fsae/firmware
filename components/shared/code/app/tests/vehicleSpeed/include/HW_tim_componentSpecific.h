#pragma once

typedef enum
{
    HW_TIM_PORT_WHEELSPEED = 0x00U,
    HW_TIM_PORT_COUNT,
} HW_TIM_port_E;

typedef enum
{
    HW_TIM_CHANNEL_WS_L = 0x00U,
    HW_TIM_CHANNEL_WS_R,
    HW_TIM_CHANNEL_WS_CNT,
} HW_TIM_channelFreq_E;

float32_t HW_TIM_getFreq(HW_TIM_channelFreq_E channel);
uint64_t  HW_TIM_getLastCaptureBaseTick(HW_TIM_channelFreq_E channel);
