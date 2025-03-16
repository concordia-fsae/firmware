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

#define ADC_MAX_COUNT 4095U // Max integer value of ADC reading (2^12 for this chip)

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    BUFFER_HALF_LOWER = 0U,
    BUFFER_HALF_UPPER,
} HW_adc_bufferHalf_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HW_StatusTypeDef_E HW_ADC_init(void);
HW_StatusTypeDef_E HW_ADC_deInit(void);
HW_StatusTypeDef_E HW_ADC_calibrate(ADC_HandleTypeDef* hadc);
HW_StatusTypeDef_E HW_ADC_startDMA(ADC_HandleTypeDef* hadc, uint32_t* data, uint32_t size);
float32_t          HW_ADC_getVFromCount(uint16_t cnt);
