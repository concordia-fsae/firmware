/**
 * @file torque.h
 * @brief Torque manager for vehicle control
 * @note Units for torque are in Nm
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
    TORQUE_INIT = 0x00U,
    TORQUE_INACTIVE,
    TORQUE_ACTIVE,
    TORQUE_ERROR,
} torque_state_E;

typedef enum
{
    LAUNCHCONTROL_INIT = 0x00U,
    LAUNCHCONTROL_INACTIVE,
    LAUNCHCONTROL_HOLDING,
    LAUNCHCONTROL_SETTLING,
    LAUNCHCONTROL_PRELOAD,
    LAUNCHCONTROL_LAUNCH,
    LAUNCHCONTROL_REJECTED,
    LAUNCHCONTROL_ERROR,
} torque_launchControl_E;

typedef enum
{
    GEAR_F = 0x00U,
    GEAR_R,
} torque_gear_E;

typedef enum
{
    RACEMODE_PIT = 0x00U,
    RACEMODE_ENABLED,
} torque_raceMode_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t                torque_getTorqueRequest(void);
float32_t                torque_getTorqueRequestMax(void);
torque_state_E           torque_getState(void);
CAN_torqueManagerState_E torque_getStateCAN(void);
torque_gear_E            torque_getGear(void);
CAN_gear_E               torque_getGearCAN(void);
torque_raceMode_E        torque_getRaceMode(void);
CAN_raceMode_E           torque_getRaceModeCAN(void);
torque_launchControl_E   torque_getLaunchControlState(void);
CAN_launchControlState_E torque_getLaunchControlStateCAN(void);
bool                     torque_isLaunching(void);
