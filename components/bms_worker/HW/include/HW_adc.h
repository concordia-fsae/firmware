/**
 * HW_adc.h
 * Header file for the ADC hardware implementation
 */
#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"

#include "stdbool.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern DMA_HandleTypeDef hdma_adc1;


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum {
    ADC_REQUEST_IO,
    ADC_REQUEST_BMS,
    ADC_REQUEST_CNT,
} ADC_Request_E; 

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
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void HW_ADC_Init(void);
bool HW_ADC_Calibrate(ADC_HandleTypeDef *hadc);
bool HW_ADC_Start_DMA(ADC_HandleTypeDef*, uint32_t*, uint32_t);
bool HW_ADC_Request_DMA(ADC_Request_E, uint32_t*);
