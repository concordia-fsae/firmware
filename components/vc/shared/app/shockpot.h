/**
 * @file shockpot.h
 * @brief Module header for shock potentiometer
 */

#pragma once

#include "LIB_Types.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/


float32_t shockpot_getLDisp(void);
float32_t shockpot_getRDisp(void);
float32_t shockpot_getLVoltage(void);
float32_t shockpot_getRVoltage(void);
