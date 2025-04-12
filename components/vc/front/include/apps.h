/**
 * @file apps.h
 * @brief Module header for the Accelerator Pedal Position Sensor
 * @note Pedal positon is a float percentage between 0.0f and 1.0f where 
 *       0.0f is 0% and 1.0f is 100%
*/

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"
#include "CANTypes_generated.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    APPS_INIT = 0x00U,
    APPS_OK,
    APPS_FAULT,
    APPS_ERROR, // Sensor failure
} apps_state_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t       apps_getPedalPosition(void);
apps_state_E    apps_getState(void);
CAN_appsState_E apps_getStateCAN(void);
