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
    DRV_INPUTAD_DIGITAL_CHANNEL_DIN1 = 0x0U,
    DRV_INPUTAD_DIGITAL_CHANNEL_DIN2,
    DRV_INPUTAD_DIGITAL_CHANNEL_DIN3,
    DRV_INPUTAD_DIGITAL_CHANNEL_DIN4,
    DRV_INPUTAD_DIGITAL_CHANNEL_DIN5,
    DRV_INPUTAD_DIGITAL_CHANNEL_DIN6,
    DRV_INPUTAD_DIGITAL_CHANNEL_DIN7,
    DRV_INPUTAD_DIGITAL_CHANNEL_DIN8,
    DRV_INPUTAD_DIGITAL_CHANNEL_DIN9,
    DRV_INPUTAD_DIGITAL_CHANNEL_DIN10,
    DRV_INPUTAD_DIGITAL_COUNT,
} drv_inputAD_channelDigital_E;

// This must match the ordering of ADC Bank 1 and Bank 2
typedef enum
{
    DRV_INPUTAD_ANALOG_CHANNEL_AIN2 = 0x0U,
    DRV_INPUTAD_ANALOG_CHANNEL_AIN4,
    DRV_INPUTAD_ANALOG_CHANNEL_AIN6,
    DRV_INPUTAD_ANALOG_CHANNEL_MCU_TEMP,
    DRV_INPUTAD_ANALOG_CHANNEL_AIN1,
    DRV_INPUTAD_ANALOG_CHANNEL_AIN3,
    DRV_INPUTAD_ANALOG_CHANNEL_AIN5,
    DRV_INPUTAD_ANALOG_CHANNEL_AIN7,
    DRV_INPUTAD_ANALOG_COUNT,
} drv_inputAD_channelAnalog_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void drv_inputAD_init_componentSpecific(void);
void drv_inputAD_1kHz_componentSpecific(void);
