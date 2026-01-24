/**
 * @file steeringAngle.h
 * @brief Module header for steering angle sensor
 */

#pragma once

#include "Module.h"
#include "LIB_Types.h"
#include "lib_nvm.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t zero;
    uint32_t spare[5];
} LIB_NVM_STORAGE(nvm_steeringCalibration_S);
extern nvm_steeringCalibration_S steeringCalibration_data;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t steeringAngle_getSteeringAngle(void);
float32_t steeringAngle_getSteeringVoltage(void);
