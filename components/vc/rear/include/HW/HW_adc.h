/**
 * @file HW_adc.h
 * @brief  Header file for ADC firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "SystemConfig.h"
#include "stdbool.h"
#include "LIB_Types.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ADC_MAX_VAL    4095U    // Max integer value of ADC reading (2^12 for this chip)
#define ADC_REF_VOLTAGE 3.0F

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

void     HW_ADC_init(void);
bool     HW_ADC_calibrate(ADC_HandleTypeDef* hadc);
bool     HW_ADC_startDMA(ADC_HandleTypeDef* hadc, uint32_t* data, uint32_t size);
uint16_t HW_ADC_getVFromCount(uint16_t cnt);
