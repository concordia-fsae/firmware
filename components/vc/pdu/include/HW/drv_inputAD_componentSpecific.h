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
    DRV_INPUTAD_TSCHG_MS = 0U,
    DRV_INPUTAD_RUN_BUTTON,
    DRV_INPUTAD_IMD_SAFETY_EN,
    DRV_INPUTAD_BMS_SAFETY_EN,
    DRV_INPUTAD_BMS_RESET,
    DRV_INPUTAD_5V_NFLT1,
    DRV_INPUTAD_5V_NFLT2,
    DRV_INPUTAD_VCU_SFTY_RESET,
    DRV_INPUTAD_BSPD_MEM,
    DRV_INPUTAD_DIGITAL_COUNT,
} drv_inputAD_channelDigital_E;

typedef enum
{
    DRV_INPUTAD_ANALOG_MUX_LP1_SNS = 0U, // Bank 1
    DRV_INPUTAD_ANALOG_MUX_LP2_SNS,
    DRV_INPUTAD_ANALOG_MUX_LP3_SNS,
    DRV_INPUTAD_ANALOG_MUX_LP4_SNS,
    DRV_INPUTAD_ANALOG_5V_VOLTAGE,
    DRV_INPUTAD_ANALOG_UVL_BATT,
    DRV_INPUTAD_ANALOG_MCU_TEMP,
    DRV_INPUTAD_ANALOG_MUX_LP5_SNS, // Bank 2
    DRV_INPUTAD_ANALOG_MUX_LP6_SNS,
    DRV_INPUTAD_ANALOG_MUX_LP7_SNS,
    DRV_INPUTAD_ANALOG_MUX_LP8_SNS,
    DRV_INPUTAD_ANALOG_MUX_LP9_SNS,
    DRV_INPUTAD_ANALOG_MUX2_HP_CS,
    DRV_INPUTAD_ANALOG_MUX2_THERMISTORS,
    DRV_INPUTAD_ANALOG_DEMUX2_PUMP,
    DRV_INPUTAD_ANALOG_DEMUX2_FAN,
    DRV_INPUTAD_ANALOG_DEMUX2_THERM_MCU,
    DRV_INPUTAD_ANALOG_DEMUX2_THERM_HSD1,
    DRV_INPUTAD_ANALOG_DEMUX2_THERM_HSD2,
    DRV_INPUTAD_ANALOG_COUNT,
} drv_inputAD_channelAnalog_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void drv_inputAD_init_componentSpecific(void);
void drv_inputAD_1kHz_componentSpecific(void);
