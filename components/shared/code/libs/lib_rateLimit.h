/**
 * lib_rateLimit.h
 * Header file for the rate limiting library
 *
 * Usage
 * 1. Configure a lib_rateLimit_X_S struct for the respective usecase
 * 2. Call lib_rateLimit_X_update() with x_n which returns y_n
 * 3. Alternately, get the current output with instance.y_n
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t y_n;
    float32_t maxStepDelta;
} lib_rateLimit_linear_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t lib_rateLimit_linear_update(lib_rateLimit_linear_S* linear, float32_t x_n);
