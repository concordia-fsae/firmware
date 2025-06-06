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

#define ADC_REF_VOLTAGE 3.0F
#define HW_ADC_BUF_LEN  96U

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

// TODO: Fillout
typedef enum
{
    ADC_BANK1_CHANNEL_AIN2,
    ADC_BANK1_CHANNEL_AIN4,
    ADC_BANK1_CHANNEL_AIN6,
    ADC_BANK1_CHANNEL_MCU_TEMP,
    ADC_BANK1_CHANNEL_COUNT,
} HW_adcChannels_bank1_E;

typedef enum
{
    ADC_BANK2_CHANNEL_AIN1,
    ADC_BANK2_CHANNEL_AIN3,
    ADC_BANK2_CHANNEL_AIN5,
    ADC_BANK2_CHANNEL_AIN7,
    ADC_BANK2_CHANNEL_COUNT,
} HW_adcChannels_bank2_E;
