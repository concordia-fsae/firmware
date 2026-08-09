/**
 * @file torque.h
 * @brief Torque manager for vehicle control
 * @note Units for torque are in Nm
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_nvm.h"
#include "LIB_Types.h"
#include "Utility.h"
#include "Yamcan.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define TC_100NM_MAX                  0.7f // Handle heavy slip conditions
#define TC_100NM_ILIM                 0.0f // Allow heavy integral limits in sustained slip with leak
#define TC_100NM_KP                   (0.591f)
#define TC_100NM_KI                   (0.0f)
#define TC_100NM_KD                   (0.070f)
#define TC_100NM_TORQUE               100U
#define TC_130NM_MAX                  0.7f // Handle heavy slip conditions
#define TC_130NM_ILIM                 0.0f // Allow heavy integral limits in sustained slip with leak
#define TC_130NM_KP                   (0.591f)
#define TC_130NM_KI                   (0.0f)
#define TC_130NM_KD                   (0.070f)
#define TC_130NM_TORQUE               130U
#define TC_150NM_MAX                  0.75f // Handle heavy slip conditions
#define TC_150NM_ILIM                 0.0f  // Allow heavy integral limits in sustained slip with leak
#define TC_150NM_KP                   (0.470f)
#define TC_150NM_KI                   (0.0f)
#define TC_150NM_KD                   (0.110f)
#define TC_150NM_TORQUE               150U
#define TC_DTERM_LPF_CUTOFF_FREQ      100
#define TC_ILEAK_MS                   500U
#define TC_SLOW_REFERENCE_RPM         10.0f

#define TC_PID_CONV_PERCENT_F32(x)    (((float32_t)x) / 100.0f)
#define TC_PID_CONV_THOU_F32(x)       (((float32_t)x) / 1000.0f)
#define TC_PID_CONV_PERCENT_U8(x)     (x * 100U)
#define TC_PID_CONV_THOU_U16(x)       (x * 1000U)
#define TC_SET_DEFAULT_SLOW_PID(decl)                                                   \
        decl                   = {                                                      \
            .percentMaxTcLimit = (uint8_t)TC_PID_CONV_PERCENT_U8(TC_130NM_MAX),         \
            .percentILim       = (uint8_t)TC_PID_CONV_PERCENT_U8(TC_130NM_ILIM),        \
            .thousandthKp      = (uint16_t)TC_PID_CONV_THOU_U16(TC_130NM_KP /           \
                                                                TC_SLOW_REFERENCE_RPM), \
            .thousandthKi      = (uint16_t)TC_PID_CONV_THOU_U16(TC_130NM_KI /           \
                                                                TC_SLOW_REFERENCE_RPM), \
            .thousandthKd      = (uint16_t)TC_PID_CONV_THOU_U16(TC_130NM_KD /           \
                                                                TC_SLOW_REFERENCE_RPM), \
            .tLeakMs           = TC_ILEAK_MS,                                           \
        };
#define TC_SET_DEFAULT_PID(decl)                                                 \
        decl                   = {                                               \
            .percentMaxTcLimit = (uint8_t)TC_PID_CONV_PERCENT_U8(TC_130NM_MAX),  \
            .percentILim       = (uint8_t)TC_PID_CONV_PERCENT_U8(TC_130NM_ILIM), \
            .thousandthKp      = (uint16_t)TC_PID_CONV_THOU_U16(TC_130NM_KP),    \
            .thousandthKi      = (uint16_t)TC_PID_CONV_THOU_U16(TC_130NM_KI),    \
            .thousandthKd      = (uint16_t)TC_PID_CONV_THOU_U16(TC_130NM_KD),    \
            .tLeakMs           = TC_ILEAK_MS,                                    \
            .maxTorqueNm       = TC_130NM_TORQUE,                                \
            .spare             = { 0U },                                         \
        };
#define TC_SET_150NM_PID(decl)                                                   \
        decl                   = {                                               \
            .percentMaxTcLimit = (uint8_t)TC_PID_CONV_PERCENT_U8(TC_150NM_MAX),  \
            .percentILim       = (uint8_t)TC_PID_CONV_PERCENT_U8(TC_150NM_ILIM), \
            .thousandthKp      = (uint16_t)TC_PID_CONV_THOU_U16(TC_150NM_KP),    \
            .thousandthKi      = (uint16_t)TC_PID_CONV_THOU_U16(TC_150NM_KI),    \
            .thousandthKd      = (uint16_t)TC_PID_CONV_THOU_U16(TC_150NM_KD),    \
            .tLeakMs           = TC_ILEAK_MS,                                    \
            .maxTorqueNm       = TC_150NM_TORQUE,                                \
            .spare             = { 0U },                                         \
        };
#define TC_SET_100NM_PID(decl)                                                   \
        decl                   = {                                               \
            .percentMaxTcLimit = (uint8_t)TC_PID_CONV_PERCENT_U8(TC_100NM_MAX),  \
            .percentILim       = (uint8_t)TC_PID_CONV_PERCENT_U8(TC_100NM_ILIM), \
            .thousandthKp      = (uint16_t)TC_PID_CONV_THOU_U16(TC_100NM_KP),    \
            .thousandthKi      = (uint16_t)TC_PID_CONV_THOU_U16(TC_100NM_KI),    \
            .thousandthKd      = (uint16_t)TC_PID_CONV_THOU_U16(TC_100NM_KD),    \
            .tLeakMs           = TC_ILEAK_MS,                                    \
            .maxTorqueNm       = TC_100NM_TORQUE,                                \
            .spare             = { 0U },                                         \
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

typedef enum
{
    TC_MAPPING_CUSTOM = 0x00U,
    TC_MAPPING_100NM,
    TC_MAPPING_150NM,
    TC_MAPPING_COUNT,
} tc_mapping_E;

// This backs our NVM parameters, each new parameter should be added before COUNT
typedef enum
{
    PARAMSTATE_TC_TIRE_MODEL_LIMIT = 0x00U,
    PARAMSTATE_COUNT,
} tc_paramState_E;

typedef struct
{
    FLAG_create(params, PARAMSTATE_COUNT);
    uint16_t selectedTcMapping;
    uint16_t spare[1];
} LIB_NVM_STORAGE(nvm_tcParamState_S);
extern nvm_tcParamState_S tcParamState_data;

typedef struct
{
    uint8_t  percentMaxTcLimit;
    uint8_t  percentILim;
    uint16_t thousandthKp;
    uint16_t thousandthKi;
    uint16_t thousandthKd;
    uint16_t tLeakMs;
} LIB_NVM_STORAGE(nvm_tcPidGains_S);

typedef struct
{
    uint8_t  percentMaxTcLimit;
    uint8_t  percentILim;
    uint16_t thousandthKp;
    uint16_t thousandthKi;
    uint16_t thousandthKd;
    uint16_t tLeakMs;
    uint16_t maxTorqueNm;
    uint16_t spare[5U];
} LIB_NVM_STORAGE(nvm_tcPid_S);

extern nvm_tcPid_S tcPid_data;
extern nvm_tcPidGains_S tcSlowPid_data;

NVM_SIZE_ASSERT(nvm_tcParamState_S, 6U);
NVM_SIZE_ASSERT(nvm_tcPidGains_S,   10U);
NVM_SIZE_ASSERT(nvm_tcPid_S,        22U);

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
float32_t                     torque_getLaunchControl75mTime(void);
torque_tractionControlState_E torque_getTractionControlState(void);
CAN_tractionControlState_E    torque_getTractionControlStateCAN(void);
bool                          tc_isParamEnabled(tc_paramState_E param);
CAN_tcMapping_E               tc_getMappingCAN(void);
float32_t                     tc_getParamPidMax(void);
float32_t                     tc_getParamILim(void);
float32_t                     tc_getParamKp(void);
float32_t                     tc_getParamKi(void);
float32_t                     tc_getParamKd(void);
float32_t                     tc_getParamTLeak(void);
