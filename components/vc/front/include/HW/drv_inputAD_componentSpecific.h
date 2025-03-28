/**
 * @file IO.h
 * @brief  Header file for IO Module
 */

#pragma once

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define DRV_INPUTAD_ADC_BUF_LEN 192U      /**< To fit the number of measurements into a time less than 100us  \
                                   the buffer length must be less than 100us / (ADC clock freq *    \
                                   cycles per conversion). For this firmware, there is 14 cycles    \
                                   per conversion. Therefore the max samples per 100us is 57, which \
                                   rounded down to the nearest multiple of 12 is 48 */

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_INPUTAD_DIGITAL_SPARE1 = 0U,
    DRV_INPUTAD_DIGITAL_SPARE2,
    DRV_INPUTAD_DIGITAL_SPARE3,
    DRV_INPUTAD_DIGITAL_SPARE4,
    DRV_INPUTAD_DIGITAL_5V_FLT2,
    DRV_INPUTAD_DIGITAL_5V_FLT1,
    DRV_INPUTAD_DIGITAL_COUNT,
} drv_inputAD_channelDigital_E;

// This must match the ordering of ADC Bank 1 and Bank 2
typedef enum
{
    DRV_INPUTAD_ANALOG_R_BR_TEMP, // Bank 1
    DRV_INPUTAD_ANALOG_L_SHK_DISP,
    DRV_INPUTAD_ANALOG_PU1,
    DRV_INPUTAD_ANALOG_BR_POT,
    DRV_INPUTAD_ANALOG_SPARE1,
    DRV_INPUTAD_ANALOG_SPARE2, 
    DRV_INPUTAD_ANALOG_APPS_P1,
    DRV_INPUTAD_ANALOG_MCU_TEMP,
    DRV_INPUTAD_ANALOG_L_BR_TEMP, // Bank 2
    DRV_INPUTAD_ANALOG_R_SHK_DISP,
    DRV_INPUTAD_ANALOG_PU2,
    DRV_INPUTAD_ANALOG_BR_PR,
    DRV_INPUTAD_ANALOG_SPARE3, 
    DRV_INPUTAD_ANALOG_SPARE4, 
    DRV_INPUTAD_ANALOG_APPS_P2,
    DRV_INPUTAD_ANALOG_BOARD_TEMP,
    DRV_INPUTAD_ANALOG_COUNT,
} drv_inputAD_channelAnalog_E;
