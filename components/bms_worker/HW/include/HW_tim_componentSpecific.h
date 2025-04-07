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
    HW_TIM_PORT_TACH,
#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
    HW_TIM_PORT_HS_INTERRUPT,
#endif
    HW_TIM_PORT_PWM,
    HW_TIM_PORT_COUNT,
} HW_TIM_port_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
void      HW_TIM_10kHz_timerStart(void);
#endif // FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK
float32_t HW_TIM1_getFreqCH1(void);
float32_t HW_TIM1_getFreqCH2(void);
