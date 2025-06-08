/**
 * @file HW_adc.h
 * @brief  Header file for ADC firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW.h"
#include "LIB_Types.h"
#include "HW_adc_componentSpecific.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ADC_MAX_COUNT 4095 // Max integer value of ADC reading (2^12 for this chip)

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    ADC_STATE_INIT = 0,
    ADC_STATE_CALIBRATION,
    ADC_STATE_RUNNING,
    ADC_STATE_CALIBRATION_FAILED,
    ADC_STATE_COUNT,
} HW_adc_state_E;

typedef enum
{
    ADC_BANK1 = 0U,
    ADC_BANK2,
    ADC_BANK_COUNT,
} HW_adc_bank_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HW_StatusTypeDef_E HW_ADC_init(void);
HW_StatusTypeDef_E HW_ADC_deInit(void);
void               HW_ADC_unpackADCBuffer(void);
float32_t          HW_ADC_getVFromBank1Channel(HW_adcChannels_bank1_E channel);
float32_t          HW_ADC_getVFromBank2Channel(HW_adcChannels_bank2_E channel);
