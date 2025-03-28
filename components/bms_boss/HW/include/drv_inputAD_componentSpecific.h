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
    DRV_INPUTAD_ANALOG_COUNT,
} drv_inputAD_channelAnalog_E;
