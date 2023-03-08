/**
 * @file HW_gpio.h
 * @brief  Header file for the firmware GPIO
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2023-01-15
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void HW_GPIO_Init(void);
void HW_CONTACTORS_CloseAll(void);
