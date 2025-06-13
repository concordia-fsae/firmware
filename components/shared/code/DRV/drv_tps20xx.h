/**
 * @file drv_tps20xx.h
 * @brief  Header file for the TI TPS20xx driver
 *
 * @note To use this driver, a "drv_tpx20xx_componentSpecific.h" file should be
 * created in a build include directory which must define the TPS20XX channels.
 * A channel is a single output from any given IC. Due to the architecture of the
 * HSD, there is no differentiation between the models where there are 1, 2, 3...
 * channels on the same chip. If there are multiple chips, all used channels
 * shall be defined in the channel configuration. This driver provides the interface
 * for application code to enable, disable, and observe the state of any TPS20XX
 * channel.
 *
 * Setup
 * 1. Define the drv_tps20xx_channel_E type in drv_tpx20xx_componentSpecific.h
 * 2. Declare the 'drv_tps20xx_channel_S drv_tps20xx_channels[DRV_TPS20XX_CHANNEL_COUNT]'
 *    array in drv_tps20xx_componentSpecific.c in the project source. Configure
 *    each channel to have the correct states
 * 3. Call the run function periodically depending on the needs of the application.
 *
 * Usage
 * - Channels are initialized in their off state
 * - To en/disable a channel, the setEnabled function will need to be called on the
 *   channel with the respective true or false for on and off respectively. The driver
 *   will handle the pin state based upon the inverted logic of the channel and
 *   the specific output pin configuration.
 * - To evaluate the current state of a given channel, the getState function
 *   shall be called
 * - Once a fault is reported by the IC, the tps20xx channel will be disabled.
 *   A faulted channel enters the over current (OC) state initially. If the
 *   channel remains faulted, the channel tripped due to an overtemperature condition. 
 *   An overtemperature condition can only be cleared once the chip has determined
 *   it has cooled down enough.
 * - When a channel is configured with auto_reset enabled, it will automatically
 *   recover from a fault after the time set in retry_wait_ms. If a channel does
 *   not have auto_reset enabled, the application shall disable the channel manually.
 *   If the channel is in a fault state, the channel will reset to
 *   the off state automatically if it has been disabled by the application and a
 *   fault is no longer present.
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_tps20xx_componentSpecific.h"
#include "drv_inputAD.h"
#include "drv_outputAD.h"
#include "stdbool.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    const drv_outputAD_channelDigital_E enable;
    const drv_inputAD_channelDigital_E  fault;
    bool                                auto_reset;
    uint16_t                            retry_wait_ms;
} drv_tps20xx_channelConfig_S;

extern drv_tps20xx_channelConfig_S drv_tps20xx_channels[DRV_TPS20XX_CHANNEL_COUNT];

typedef enum
{
    DRV_TPS20XX_STATE_INIT = 0x00U,
    DRV_TPS20XX_STATE_OFF,
    DRV_TPS20XX_STATE_ENABLED,
    DRV_TPS20XX_STATE_FAULTED_OC,
    DRV_TPS20XX_STATE_FAULTED_OT,
    DRV_TPS20XX_STATE_RETRY,
    DRV_TPS20XX_STATE_ERROR,
} drv_tps20xx_state_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HW_StatusTypeDef_E  drv_tps20xx_init(void);
HW_StatusTypeDef_E  drv_tps20xx_deInit(void);
void                drv_tps20xx_run(void);

drv_tps20xx_state_E drv_tps20xx_getState(drv_tps20xx_channel_E channel);
CAN_hsdState_E      drv_tps20xx_getStateCAN(drv_tps20xx_channel_E channel);
void                drv_tps20xx_setEnabled(drv_tps20xx_channel_E channel, bool enabled);
