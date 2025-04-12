/**
 * @file brakeLight.h
 * @brief Module that manages the brake light
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
    BRAKELIGHT_INIT = 0x00U,
    BRAKELIGHT_OFF,
    BRAKELIGHT_ON,
    BRAKELIGHT_FAULT,
} brakeLight_state_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool                  brakeLight_isOn(void);
brakeLight_state_E    brakeLight_getState(void);
CAN_brakeLightState_E brakeLight_getStateCAN(void);
