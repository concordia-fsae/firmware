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
#define HW_ADC_BUF_LEN  48U

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    ADC_BANK1_CHANNEL_MCU_TEMP = 0x00U,
    ADC_BANK1_CHANNEL_MUX1,
    ADC_BANK1_CHANNEL_MUX2,
    ADC_BANK1_CHANNEL_MUX3,
    ADC_BANK1_CHANNEL_BOARD1,
    ADC_BANK1_CHANNEL_BOARD2,
    ADC_BANK1_CHANNEL_COUNT,
} HW_adcChannels_bank1_E;

typedef enum
{
    ADC_BANK2_CHANNEL_BMS_CHIP = 0x00U,
    ADC_BANK2_CHANNEL_COUNT,
} HW_adcChannels_bank2_E;
