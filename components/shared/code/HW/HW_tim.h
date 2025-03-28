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
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    HW_TIM_CHANNEL_1 = 0x00U,
    HW_TIM_CHANNEL_2,
    HW_TIM_CHANNEL_3,
    HW_TIM_CHANNEL_4,
} HW_TIM_channel_E;

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

void               HW_TIM_periodElapsedCb(TIM_HandleTypeDef* tim);
void               HW_TIM_setDuty(HW_TIM_port_E port, HW_TIM_channel_E channel, float32_t percentage);
