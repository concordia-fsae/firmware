/**
 * @file HW_clock.h
 * @brief  Header for Aero Sensor clock firmware
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-02
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stm32f1xx_hal.h"
#include "ErrorHandler.h"


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void SystemClock_Config(void);
void HW_Delay(uint32_t delay);
uint32_t HW_GetTick(void);

