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

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define HW_TIM_TICK TIM2
#define HW_TIM_TICK_IRQN TIM2_IRQn
#define HW_TIM_TICK_ENABLECLK __HAL_RCC_TIM2_CLK_ENABLE
#define HW_TIM_TICK_GETCLKFREQ 2*HAL_RCC_GetPCLK1Freq

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    HW_TIM_PORT_COUNT,
} HW_TIM_port_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

uint16_t HW_TIM1_getFreqCH1(void);
uint16_t HW_TIM3_getFreqCH1(void);
