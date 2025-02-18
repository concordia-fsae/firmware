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
#include "HW_tim_componentSpecific.h"

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HW_StatusTypeDef_E HW_TIM_init(void);
HW_StatusTypeDef_E HW_TIM_deInit(void);
void               HW_TIM_configureRunTimeStatsTimer(void);
uint32_t           HW_TIM_getTick(void);
uint32_t           HW_TIM_getTimeMS(void);
void               HW_TIM_incBaseTick(void);
uint64_t           HW_TIM_getBaseTick(void);
void               HW_TIM_delayMS(uint32_t delay);
void               HW_TIM_delayUS(uint8_t us);

void               HW_TIM_periodElapsedCb(TIM_HandleTypeDef* htim);
