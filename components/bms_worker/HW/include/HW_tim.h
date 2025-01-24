/**
 * @file HW_tim.h
 * @brief  Header file for TIM firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW.h"
#include "stm32f1xx_hal.h"
#include "FeatureDefines_generated.h"


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HW_StatusTypeDef_E HW_TIM_init(void);
HW_StatusTypeDef_E HW_TIM_deInit(void);
void               HW_TIM_configureRunTimeStatsTimer(void);
void               HW_TIM_incBaseTick(void);
uint64_t           HW_TIM_getBaseTick(void);
uint32_t           HW_TIM_getTick(void);
void               HW_TIM_delayMS(uint32_t delay);
void               HW_TIM_delayUS(uint8_t us);
uint32_t           HW_TIM_getTimeMS(void);
#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
void               HW_TIM_10kHz_timerStart(void);
#endif // FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK
void               HW_TIM4_setDutyCH1(uint8_t);
void               HW_TIM4_setDutyCH2(uint8_t);
uint16_t           HW_TIM1_getFreqCH1(void);
uint16_t           HW_TIM1_getFreqCH2(void);
