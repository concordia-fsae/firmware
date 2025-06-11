/**
 * @file IO.h
 * @brief  Header file for IO Module
 */

#pragma once

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_INPUTAD_DIGITAL_TSMS_CHG,
    DRV_INPUTAD_DIGITAL_OK_HS,
    DRV_INPUTAD_DIGITAL_BMS_IMD_RESET,
    DRV_INPUTAD_DIGITAL_IMD_STATUS_MEM,
    DRV_INPUTAD_DIGITAL_BMS_STATUS_MEM,
    DRV_INPUTAD_DIGITAL_COUNT,
} drv_inputAD_channelDigital_E;

typedef enum
{
    DRV_INPUTAD_ANALOG_CS,
    DRV_INPUTAD_ANALOG_MCU_TEMP,
#if BMSB_CONFIG_ID == 1U
    DRV_INPUTAD_ANALOG_VPACK,
#endif
    DRV_INPUTAD_ANALOG_COUNT,
} drv_inputAD_channelAnalog_E;
