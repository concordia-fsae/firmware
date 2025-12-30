/**
 * @file app_vehicleState.h
 * @brief Header file for the vehicle state application
 * @note This application controls the vehicle wide power and drive state transitions
 *       as ensuring their functionality
 *
 * Setup - Leader
 * 1. Define the `VEHICLESTATE_INPUTAD_TSMS` channel
 * 2. Define the `VEHICLESTATE_INPUTAD_RUN_BUTTON` channel
 * 3. Define the `VEHICLESTATE_CANRX_CONTACTORSTATE` signal
 * 4. Update the module manager with the app_vehicleState_desc
 *
 * Setup - Follower
 * 1. Define the `VEHICLESTATE_CANRX_SIGNAL` channel
 * 2. Update the module manager with the app_vehicleState_desc
 *
 * Usage
 * - The module shall be call periodically at 100Hz
 * - The module provides the app_vehicleState_getState for all applications
 *   to have the state correctly tracke irregardless of leader/follower
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "app_vehicleState_componentSpecific.h"
#include "LIB_Types.h"
#include "Module.h"
#include "CANTypes_generated.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#if FEATURE_VEHICLESTATE_MODE == FDEFS_MODE_LEADER
 #ifndef VEHICLESTATE_INPUTAD_TSMS
 #error "User must define 'VEHICLESTATE_INPUTAD_TSMS' in app_vehicleState_componentSpecific.h"
 #endif
 #ifndef VEHICLESTATE_INPUTAD_RUN_BUTTON
 #error "User must define 'VEHICLESTATE_INPUTAD_RUN_BUTTON' in app_vehicleState_componentSpecific.h"
 #endif
 #ifndef VEHICLESTATE_CANRX_CONTACTORSTATE
 #error "User must define 'VEHICLESTATE_CANRX_CONTACTORSTATE' in app_vehicleState_componentSpecific.h"
 #endif
 #ifndef VEHICLESTATE_CANRX_BRAKEPOSITION
 #error "User must define 'VEHICLESTATE_CANRX_BRAKEPOSITION' in app_vehicleState_componentSpecific.h"
 #endif
#else
 #ifndef VEHICLESTATE_CANRX_SIGNAL
 #error "User must define 'VEHICLESTATE_CANRX_SIGNAL' in app_vehicleState_componentSpecific.h"
 #endif
#endif

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern const ModuleDesc_S app_vehicleState_desc;

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

/**
 * @brief Tracks the vehicle wide power state
 * @value VEHICLE_STATE_ON_GLV Only GLV power is presently turned on. HV is down,
 *        however the vehicle may be ready for HV to be turned on at any point.
 * @value VEHICLE_STATE_ON_HV HV has been energized. All critical systems shall be
 *        on and in a nominal state. Only critical faults and user input can force
 *        a state transition.
 * @value VEHICLE_STATE_TS_RUN The vehicle can respond to driver input. The motor
 *        controller has been enabled and will respond to torque requests. Only
 *        critical faults and exiting 'DRIVE' can result in a state transition.
 */
typedef enum
{
    VEHICLESTATE_INIT = 0x00,
    VEHICLESTATE_ON_GLV,
    VEHICLESTATE_ON_HV,
    VEHICLESTATE_TS_RUN,
    VEHICLESTATE_SLEEP,
} app_vehicleState_state_E;

typedef enum
{
    SLEEPABLE_OK = 0x00U,
    SLEEPABLE_NOK,
    SLEEPABLE_ALARM,
} app_vehicleState_sleepable_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

// Functionality
void app_vehicleState_init(void);
void app_vehicleState_run100Hz(void);
void app_vehicleState_delaySleep(uint32_t ms);
// Accessors
bool                     app_vehicleState_sleeping(void);
bool                     app_vehicleState_getFaultReset(void);
app_vehicleState_state_E app_vehicleState_getState(void);
CAN_sleepFollowerState_E app_vehicleState_getSleepableStateCAN(void);
#if FEATURE_VEHICLESTATE_MODE == FDEFS_MODE_LEADER
CAN_vehicleState_E       app_vehicleState_getStateCAN(void);
#endif
