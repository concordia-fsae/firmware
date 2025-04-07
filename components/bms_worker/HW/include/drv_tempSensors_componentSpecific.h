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
    DRV_TEMPSENSORS_CHANNEL_MCU_TEMP = 0x00U,
    DRV_TEMPSENSORS_CHANNEL_BOARD1,
    DRV_TEMPSENSORS_CHANNEL_BOARD2,
    DRV_TEMPSENSORS_CHANNEL_SEGMENT_MAX,
    DRV_TEMPSENSORS_CHANNEL_COUNT,
} drv_tempSensors_channel_E;
