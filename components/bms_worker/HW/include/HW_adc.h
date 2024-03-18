/**
 * @file HW_adc.h
 * @brief  Header file for ADC firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW.h"

#include "SystemConfig.h"
#include "stdbool.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    BUFFER_HALF_LOWER = 0U,
    BUFFER_HALF_UPPER,
} bufferHalf_E;

typedef struct
{
    uint32_t  raw;
    float32_t value;
    uint16_t  count;
} simpleFilter_S;


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern DMA_HandleTypeDef hdma_adc1;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HW_StatusTypeDef_E HW_ADC_init(void);
HW_StatusTypeDef_E HW_ADC_deInit(void);
bool HW_ADC_calibrate(ADC_HandleTypeDef *hadc);
bool HW_ADC_startDMA(ADC_HandleTypeDef*, uint32_t*, uint32_t); 
uint16_t HW_ADC_getVFromCount(uint16_t cnt);