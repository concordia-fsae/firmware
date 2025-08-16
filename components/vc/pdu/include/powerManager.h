/**
 * @file powerManager.h
 * @brief Header file for VCPDU power manager
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t powerManager_getGLVVoltage(void);
float32_t powerManager_getGLVCurrent(void);
float32_t powerManager_getPduCurrent(void);
