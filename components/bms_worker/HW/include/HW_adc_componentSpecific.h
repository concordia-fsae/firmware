/**
 * @file HW_adc_componentSpecific.h
 * @brief  Header file for ADC firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW.h"
#include "BuildDefines.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ADC_REF_VOLTAGE 3.0f
#define HW_ADC_BUF_LEN  48U

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    ADC_BANK1_CHANNEL_TEMP_MCU = 0x00U,
    ADC_BANK1_CHANNEL_MUX1,
#if APP_VARIANT_ID == 0U
    ADC_BANK1_CHANNEL_MUX2,
    ADC_BANK1_CHANNEL_MUX3,
#endif
    ADC_BANK1_CHANNEL_TEMP_BALANCING1,
    ADC_BANK1_CHANNEL_TEMP_BALANCING2,
#if APP_VARIANT_ID == 1U
    ADC_BANK1_CHANNEL_TEMP_BOARD,
    ADC_BANK1_CHANNEL_TEMP_THERM9,
#endif
    ADC_BANK1_CHANNEL_COUNT,
} HW_adcChannels_bank1_E;

typedef enum
{
    ADC_BANK2_CHANNEL_BMS_CHIP = 0x00U,
    ADC_BANK2_CHANNEL_COUNT,
} HW_adcChannels_bank2_E;
