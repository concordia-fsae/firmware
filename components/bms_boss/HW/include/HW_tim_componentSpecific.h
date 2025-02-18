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

#define HW_TIM_TICK TIM2
#define HW_TIM_TICK_IRQN TIM2_IRQn
#define HW_TIM_TICK_ENABLECLK __HAL_RCC_TIM2_CLK_ENABLE
#define HW_TIM_TICK_GETCLKFREQ 2*HAL_RCC_GetPCLK1Freq

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

uint16_t HW_TIM1_getFreqCH1(void);
uint16_t HW_TIM3_getFreqCH1(void);
