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

#define ADC_BANK1_CHANNEL_COUNT ADC_BANK_CHANNEL_COUNT
#define ADC_BANK2_CHANNEL_COUNT ADC_BANK_CHANNEL_COUNT

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    ADC_BANK_CHANNEL_CS,
#if BMSB_CONFIG_ID == 1U
    ADC_BANK_CHANNEL_VPACK,
#endif
    ADC_BANK_CHANNEL_MCU_TEMP,
    ADC_BANK_CHANNEL_COUNT,
} HW_adcChannels_bank1_E;
typedef HW_adcChannels_bank1_E HW_adcChannels_bank2_E;
