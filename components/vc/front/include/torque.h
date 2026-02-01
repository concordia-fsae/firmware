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
    LC_STATE_INIT = 0x00U,
    LC_STATE_INACTIVE,
    LC_STATE_HOLDING,
    LC_STATE_SETTLING,
    LC_STATE_PRELOAD,
    LC_STATE_LAUNCH,
    LC_STATE_REJECTED,
    LC_STATE_ERROR,
} torque_launchControlState_E;

typedef enum
{
    TC_STATE_INIT = 0x00U,
    TC_STATE_INACTIVE,
    TC_STATE_ACTIVE,
    TC_STATE_FAULT_SENSOR,
    TC_STATE_ERROR,
    TC_STATE_LOCKOUT,
} torque_tractionControlState_E;

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

float32_t                     torque_getTorqueRequest(void);
float32_t                     torque_getTorqueRequestMax(void);
float32_t                     torque_getTorqueRequestCorrection(void);
float32_t                     torque_getTorqueDriverInput(void);
float32_t                     torque_getPreloadTorque(void);
float32_t                     torque_getSlipRaw(void);
float32_t                     torque_getSlipTarget(void);
float32_t                     torque_getSlipErrorP(void);
float32_t                     torque_getSlipErrorI(void);
float32_t                     torque_getSlipErrorD(void);
float32_t                     torque_getTorqueReduction(void);
torque_state_E                torque_getState(void);
CAN_torqueManagerState_E      torque_getStateCAN(void);
torque_gear_E                 torque_getGear(void);
CAN_gear_E                    torque_getGearCAN(void);
torque_raceMode_E             torque_getRaceMode(void);
CAN_raceMode_E                torque_getRaceModeCAN(void);
torque_launchControlState_E   torque_getLaunchControlState(void);
CAN_launchControlState_E      torque_getLaunchControlStateCAN(void);
bool                          torque_isLaunching(void);
torque_tractionControlState_E torque_getTractionControlState(void);
CAN_tractionControlState_E    torque_getTractionControlStateCAN(void);
