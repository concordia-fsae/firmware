/**
 * @file HW_adc_componentSpecific.h
 * @brief  Header file for ADC firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ADC_REF_VOLTAGE 2.5F
#define HW_ADC_BUF_LEN  98U

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    ADC_BANK1_CHANNEL_MUX_LP1_SNS,
    ADC_BANK1_CHANNEL_MUX_LP2_SNS,
    ADC_BANK1_CHANNEL_MUX_LP3_SNS,
    ADC_BANK1_CHANNEL_MUX_LP4_SNS,
    ADC_BANK1_CHANNEL_5V_VOLTAGE,
    ADC_BANK1_CHANNEL_UVL_BATT,
    ADC_BANK1_CHANNEL_MCU_TEMP,
    ADC_BANK1_CHANNEL_COUNT,
} HW_adcChannels_bank1_E;

typedef enum
{
    ADC_BANK1_CHANNEL_MUX_LP5_SNS,
    ADC_BANK2_CHANNEL_MUX_LP6_SNS,
    ADC_BANK2_CHANNEL_MUX_LP7_SNS,
    ADC_BANK2_CHANNEL_MUX_LP8_SNS,
    ADC_BANK2_CHANNEL_MUX_LP9_SNS,
    ADC_BANK2_CHANNEL_MUX2_HP_CS,
    ADC_BANK2_CHANNEL_MUX2_THERMISTORS,
    ADC_BANK2_CHANNEL_COUNT,
} HW_adcChannels_bank2_E;
