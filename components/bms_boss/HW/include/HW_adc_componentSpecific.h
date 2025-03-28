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

#define ADC_REF_VOLTAGE 3.0f
#define HW_ADC_BUF_LEN  96U

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    ADC_BANK1_CHANNEL_CS_N,
    ADC_BANK1_CHANNEL_MCU_TEMP,
    ADC_BANK1_CHANNEL_COUNT,
} HW_adcChannels_bank1_E;

typedef enum
{
    ADC_BANK2_CHANNEL_CS_P = 0x00U,
    // All empty values should be disregarded because of simultaneous current sense sampling
    ADC_BANK2_CHANNEL_COUNT = ADC_BANK1_CHANNEL_COUNT,
} HW_adcChannels_bank2_E;
