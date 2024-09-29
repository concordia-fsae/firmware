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

HW_StatusTypeDef_E HW_init(void);
HW_StatusTypeDef_E HW_deInit(void);
void     HW_systemHardReset(void);
