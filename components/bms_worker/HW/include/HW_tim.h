/**
 * @file HW_tim.h
 * @brief  Header file for TIM firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "stm32f1xx_hal.h"


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HAL_StatusTypeDef HW_TIM_init(void);
void              HW_TIM_configureRunTimeStatsTimer(void);
void              HW_TIM_incBaseTick(void);
uint64_t          HW_TIM_getBaseTick(void);

#if defined(BMSW_BOARD_VA1)
void HW_TIM1_setDuty(uint8_t);
#elif defined(BMSW_BOARD_VA3)    // BMSW_BOARD_VA1
void     HW_TIM4_setDutyCH1(uint8_t);
void     HW_TIM4_setDutyCH2(uint8_t);
uint16_t HW_TIM1_getFreqCH1(void);
uint16_t HW_TIM1_getFreqCH2(void);
#endif                           // BMSW_BOARD_VA3
