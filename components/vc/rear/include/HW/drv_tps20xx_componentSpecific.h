/**
 * @file drv_tps20xx_componentSpecific.h
 * @brief  Component specific header file for the TI TPS20xx* driver
 */

#pragma once

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_TPS20XX_CHANNEL_5V_CRITICAL = 0x00U,
    DRV_TPS20XX_CHANNEL_5V_EXT,
    DRV_TPS20XX_CHANNEL_COUNT,
} drv_tps20xx_channel_E;
