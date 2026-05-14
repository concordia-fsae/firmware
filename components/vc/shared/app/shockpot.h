/**
 * @file shockpot.h
 * @brief Module header for shock potentiometer
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    SHOCKPOT_LEFT,
    SHOCKPOT_RIGHT,
    SHOCKPOT_COUNT,
} shockpot_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t shockpot_getDisplacement(shockpot_E pot);
float32_t shockpot_getVoltage(shockpot_E pot);
