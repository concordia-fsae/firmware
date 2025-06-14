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
        .cs_amp_per_volt = 0.75f,
        .cs_channel = DRV_INPUTAD_ANALOG_DEMUX2_PUMP,
        .fault_reset = DRV_OUTPUTAD_PUMP_FAULT,
        .enable = DRV_OUTPUTAD_PUMP_EN,
        .enable_cs = DRV_OUTPUTAD_HP_SNS_EN,
    },
    [DRV_VN9008_CHANNEL_FAN] = {
        .cs_amp_per_volt = 0.75f,
        .cs_channel = DRV_INPUTAD_ANALOG_DEMUX2_FAN,
        .fault_reset = DRV_OUTPUTAD_FAN_FAULT,
        .enable = DRV_OUTPUTAD_FAN_EN,
        .enable_cs = DRV_OUTPUTAD_HP_SNS_EN,
    },
};
