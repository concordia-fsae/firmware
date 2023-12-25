/**
 * HW_adc.h
 * Header file for the ADC hardware implementation
 */
#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/
extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void HW_ADC_Init(void);
HAL_StatusTypeDef HAL_ADC_Calibrate(ADC_HandleTypeDef *hadc);
