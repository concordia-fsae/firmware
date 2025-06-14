/**
 * @file drv_outputAD_componentSpecific.h
 * @brief Header file for the component specific output driver
 */

#pragma once

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_OUTPUTAD_ANALOG_COUNT,
} drv_outputAD_channelAnalog_E;

typedef enum
{
    DRV_OUTPUTAD_DIGITAL_STATUS_BMS = 0x00U,
#if BMSB_CONFIG_ID == 0U
    DRV_OUTPUTAD_DIGITAL_STATUS_IMD,
#endif
    DRV_OUTPUTAD_DIGITAL_AIR,
    DRV_OUTPUTAD_DIGITAL_PRECHG,
    DRV_OUTPUTAD_DIGITAL_LED,
    DRV_OUTPUTAD_DIGITAL_COUNT,
} drv_outputAD_channelDigital_E;
