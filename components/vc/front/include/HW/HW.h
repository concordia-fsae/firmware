/**
 * @file HW.h
 * @brief  Header file for generic firmware functions
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stm32f1xx.h"

// System Includes
#include "LIB_Types.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum 
{
  HW_OK = 0x00U,
  HW_ERROR = 0x01U
} HW_StatusTypeDef_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool     HW_init(void);
uint32_t HW_getTick(void);
void     HW_delay(uint32_t delay);
void     HW_usDelay(uint8_t us);
void     HW_systemHardReset(void);
void     Error_Handler(void);
