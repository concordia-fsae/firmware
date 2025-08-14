/**
 * @file steeringAngle.h
 * @brief Module header for steering angle sensor
 */

#pragma once

#include "Module.h"
#include "LIB_Types.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t steeringAngle_getSteeringAngle(void);
float32_t steeringAngle_getSteeringVoltage(void);