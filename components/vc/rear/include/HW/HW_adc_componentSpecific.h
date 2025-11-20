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

typedef enum
{
    ADC_BANK1_CHANNEL_R_BR_TEMP,
    ADC_BANK1_CHANNEL_L_SHK_DISP,
    ADC_BANK1_CHANNEL_PU1,
    ADC_BANK1_CHANNEL_BR_POT,
    ADC_BANK1_CHANNEL_SPARE1,
    ADC_BANK1_CHANNEL_SPARE2, 
    ADC_BANK1_CHANNEL_APPS_P1,
    ADC_BANK1_CHANNEL_MCU_TEMP,
    ADC_BANK1_CHANNEL_COUNT,
    ADC_BANK2_CHANNEL_BRK_TEMP,
} HW_adcChannels_bank1_E;

typedef enum
{
    ADC_BANK2_CHANNEL_L_BR_TEMP,
    ADC_BANK2_CHANNEL_R_SHK_DISP,
    ADC_BANK2_CHANNEL_PU2,
    ADC_BANK2_CHANNEL_BR_PR,
    ADC_BANK2_CHANNEL_SPARE3, 
    ADC_BANK2_CHANNEL_SPARE4, 
    ADC_BANK2_CHANNEL_APPS_P2,
    ADC_BANK2_CHANNEL_BOARD_TEMP,
    ADC_BANK2_CHANNEL_COUNT,
} HW_adcChannels_bank2_E;
