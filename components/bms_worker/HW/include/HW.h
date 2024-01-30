/**
 * @file HW.h
 * @brief  Header file for generic firmware functions
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "stdbool.h"
#include "stdint.h"


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool     HW_Init(void);
uint32_t HW_GetTick(void);
void     HW_Delay(uint32_t delay);
void     HW_usDelay(uint8_t us);
