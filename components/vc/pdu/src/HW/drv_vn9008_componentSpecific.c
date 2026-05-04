/**
* @file drv_vn9008_componentSpecific.c
* @brief Source file for the component specific VN9008 driver
*/
/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_vn9008.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

const drv_vn9008_channelConfig_S drv_vn9008_channels[DRV_VN9008_CHANNEL_COUNT] = {
    [DRV_VN9008_CHANNEL_PUMP] = {
        .type = VN9008_PWM_EN,
        .enable = {
            .pwm_en = {
                .pwm = {
                    .tim_port = HW_TIM_PORT_HP,
                    .tim_channel = HW_TIM_CHANNEL_1,
                },
                .en = DRV_OUTPUTAD_PWM1,
            },
        },
        .cs_amp_per_volt = 7.518f,
        .cs_channel = DRV_INPUTAD_ANALOG_DEMUX2_PUMP,
        .fault_reset = DRV_OUTPUTAD_PUMP_FAULT,
        .enable_cs = DRV_OUTPUTAD_HP_SNS_EN,
        .current_limit_amp = 20.0f,
        .oc_timeout_ms = 500,
    },
    [DRV_VN9008_CHANNEL_FAN] = {
        .type = VN9008_PWM_EN,
        .enable = {
            .pwm_en = {
                .pwm = {
                    .tim_port = HW_TIM_PORT_HP,
                    .tim_channel = HW_TIM_CHANNEL_2,
                },
                .en = DRV_OUTPUTAD_FAN_EN,
            },
        },
        .cs_amp_per_volt = 7.518f,
        .cs_channel = DRV_INPUTAD_ANALOG_DEMUX2_FAN,
        .fault_reset = DRV_OUTPUTAD_FAN_FAULT,
        .enable_cs = DRV_OUTPUTAD_HP_SNS_EN,
        .current_limit_amp = 15.0f,
        .oc_timeout_ms = 250,
    },
};
