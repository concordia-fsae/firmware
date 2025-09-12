/**
 * @file brakePressure.h
 * @brief Module header for brake pressure sensor
 */

#pragma once

#include "Module.h"


/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

/***************************s***************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t brakePressure_getBrakePressure(void);
float32_t brakePressure_getBrakeBias(void);// Brake bias in this situation is expressed as the percentage of force applied to the front brakes
