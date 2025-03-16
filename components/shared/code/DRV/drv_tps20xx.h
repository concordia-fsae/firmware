/**
 * @file drv_tps20xx.h
 * @brief  Header file for the TI TPS20xx driver
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_tps20xx_componentSpecific.h"
#include "HW_gpio.h"
#include "stdbool.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    const HW_GPIO_pinmux_E enable;
    const bool             inverted_enable_logic;
    const HW_GPIO_pinmux_E fault;
    bool                   auto_reset;
    uint16_t               retry_wait_ms;
} drv_tps20xx_channel_S;

extern drv_tps20xx_channel_S drv_tps20xx_channels[DRV_TPS20XX_CHANNEL_COUNT];

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
void                drv_tps20xx_run100ms(void);

drv_tps20xx_state_E drv_tps20xx_getState(drv_tps20xx_channel_E channel);
void                drv_tps20xx_setEnabled(drv_tps20xx_channel_E channel, bool enabled);
