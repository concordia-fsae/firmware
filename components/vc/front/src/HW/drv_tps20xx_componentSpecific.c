/**
 * @file drv_tps20xx_componentSpecific.
 * @brief  Component specific source file for the TI TPS20xx* driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_tps20xx.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

drv_tps20xx_channelConfig_S drv_tps20xx_channels[DRV_TPS20XX_CHANNEL_COUNT] = {
    [DRV_TPS20XX_CHANNEL_5V_CRITICAL] = {
        .enable = DRV_OUTPUTAD_DIGITAL_5V_EN1,
        .fault = DRV_INPUTAD_DIGITAL_5V_FLT1,
        .auto_reset = false,
        .retry_wait_ms = 1U, // We want this line to come back on as soon as possible
    },
    [DRV_TPS20XX_CHANNEL_5V_EXT] = {
        .enable = DRV_OUTPUTAD_DIGITAL_5V_EN2,
        .fault = DRV_INPUTAD_DIGITAL_5V_FLT2,
        .auto_reset = false,
        .retry_wait_ms = 1000U,
    },
};
