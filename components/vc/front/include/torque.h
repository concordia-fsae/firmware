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
#include "Yamcan.h"
#include "Utility.h"
#include "lib_nvm.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define TC_MAX 0.7f // Handle heavy slip conditions
#define TC_ILIM 0.55f // Allow heavy integral limits in sustained slip with leak
// Cutoff: TC_MAX. Point of full cutoff: 50% slip error
// 70% aggressivity (vibes)
#define TC_KP ((TC_MAX / 0.5f) * 0.7f)
// Ki = Kp / tIntegrator
// Ki = TC_KP / 0.250
#define TC_KI (4 * TC_KP)
#define TC_KD 0.0f
#define TC_ILEAK_MS 500U

#define TC_PID_CONV_PERCENT_F32(x) (((float32_t)x) / 100.0f)
#define TC_PID_CONV_THOU_F32(x) (((float32_t)x) / 1000.0f)
#define TC_PID_CONV_PERCENT_U8(x) (x * 100U)
#define TC_PID_CONV_THOU_U16(x) (x * 1000U)
#define TC_SET_DEFAULT_PID(decl) \
    decl = { \
        .percentMaxTcLimit = (uint8_t)TC_PID_CONV_PERCENT_U8(TC_MAX), \
        .percentILim = (uint8_t)TC_PID_CONV_PERCENT_U8(TC_ILIM), \
        .thousandthKp = (uint16_t)TC_PID_CONV_THOU_U16(TC_KP), \
        .thousandthKi = (uint16_t)TC_PID_CONV_THOU_U16(TC_KI), \
        .thousandthKd = (uint16_t)TC_PID_CONV_THOU_U16(TC_KD), \
        .tLeakMs = TC_ILEAK_MS, \
        .spare = { 0U }, \
    };

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

// This backs our NVM parameters, each new parameter should be added before COUNT
typedef enum
{
    PARAMSTATE_TC_TIRE_MODEL_LIMIT = 0x00U,
    PARAMSTATE_COUNT,
} tc_paramState_E;

typedef struct
{
    FLAG_create(params, PARAMSTATE_COUNT);
    uint16_t spare[2];
} LIB_NVM_STORAGE(nvm_tcParamState_S);
extern nvm_tcParamState_S tcParamState_data;

typedef struct
{
    uint8_t percentMaxTcLimit;
    uint8_t percentILim;
    uint16_t thousandthKp;
    uint16_t thousandthKi;
    uint16_t thousandthKd;
    uint16_t tLeakMs;
    uint16_t spare[6U];
} LIB_NVM_STORAGE(nvm_tcPid_S);
extern nvm_tcPid_S tcPid_data;

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
float32_t                     torque_getVdMaxTorqueRequest(void);
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
bool                          tc_isParamEnabled(tc_paramState_E param);
