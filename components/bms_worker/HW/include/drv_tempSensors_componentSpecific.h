/**
 * @file drv_tempSensors_componentSpecific.h
 * @brief  Header file for the component specific temperature driver
 */

#pragma once

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_TEMPSENSORS_CHANNEL_MCU = 0x00U,
    DRV_TEMPSENSORS_CHANNEL_BALANCING1,
    DRV_TEMPSENSORS_CHANNEL_BALANCING2,
#if APP_VARIANT_ID == 1U
    DRV_TEMPSENSORS_CHANNEL_BOARD,
#endif
    DRV_TEMPSENSORS_CHANNEL_SEGMENT_MAX,
    DRV_TEMPSENSORS_CHANNEL_COUNT,
} drv_tempSensors_channel_E;
