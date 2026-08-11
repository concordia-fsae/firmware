#pragma once

#include "LIB_Types.h"

typedef enum
{
    CANRX_MESSAGE_SNA = 0U,
    CANRX_MESSAGE_VALID,
    CANRX_MESSAGE_MIA,
} CANRX_MESSAGE_health_E;

typedef enum
{
    CAN_BUS_VEH = 0U,
    VEH         = CAN_BUS_VEH,
    CAN_BUS_COUNT,
} CAN_bus_E;

typedef enum
{
    SWS_requestTcKpInc = 0U,
    SWS_requestTcKpDec,
    SWS_requestTcKiInc,
    SWS_requestTcKiDec,
    SWS_requestTcKdInc,
    SWS_requestTcKdDec,
    SWS_requestTcMaxLimInc,
    SWS_requestTcMaxLimDec,
    SWS_requestTcILimInc,
    SWS_requestTcILimDec,
    SWS_requestTcTLeakMsInc,
    SWS_requestTcTLeakMsDec,
    SWS_requestTcTireModelLimit,
    SWS_requestTcMappingInc,
    SWS_requestTcMappingDec,
    SWS_requestReverse,
    SWS_requestRaceMode,
    SWS_requestTorqueInc,
    SWS_requestTorqueDec,
    SWS_requestLaunchControl,
    SWS_requestTractionControl,
    SWS_requestRegenEnabled,
    SWS_requestSlipInc,
    SWS_requestSlipDec,
    SWS_requestPreloadTorqueInc,
    SWS_requestPreloadTorqueDec,
    VCPDU_lon,
    VCREAR_brakePressure,
    VCPDU_vehicleState,
    VCPDU_safetyReset,
} CAN_signal_E;

typedef enum
{
    CAN_DIGITALSTATUS_SNA = 0U,
    CAN_DIGITALSTATUS_OFF,
    CAN_DIGITALSTATUS_ON,
} CAN_digitalStatus_E;

typedef enum
{
    CAN_TORQUEMANAGERSTATE_SNA = 0U,
    CAN_TORQUEMANAGERSTATE_INACTIVE,
    CAN_TORQUEMANAGERSTATE_ACTIVE,
} CAN_torqueManagerState_E;

typedef enum
{
    CAN_GEAR_SNA = 0U,
    CAN_GEAR_FORWARD,
    CAN_GEAR_REVERSE,
} CAN_gear_E;

typedef enum
{
    CAN_RACEMODE_PIT = 0U,
    CAN_RACEMODE_RACE,
} CAN_raceMode_E;

typedef enum
{
    CAN_LAUNCHCONTROLSTATE_SNA = 0U,
    CAN_LAUNCHCONTROLSTATE_INACTIVE,
    CAN_LAUNCHCONTROLSTATE_HOLDING,
    CAN_LAUNCHCONTROLSTATE_SETTLING,
    CAN_LAUNCHCONTROLSTATE_PRELOAD,
    CAN_LAUNCHCONTROLSTATE_LAUNCH,
    CAN_LAUNCHCONTROLSTATE_REJECTED,
    CAN_LAUNCHCONTROLSTATE_ERROR,
} CAN_launchControlState_E;

typedef enum
{
    CAN_TRACTIONCONTROLSTATE_SNA = 0U,
    CAN_TRACTIONCONTROLSTATE_INACTIVE,
    CAN_TRACTIONCONTROLSTATE_ACTIVE,
    CAN_TRACTIONCONTROLSTATE_FAULT_SENSOR,
    CAN_TRACTIONCONTROLSTATE_ERROR,
    CAN_TRACTIONCONTROLSTATE_LOCKOUT,
} CAN_tractionControlState_E;

typedef enum
{
    CAN_TCMAPPING_SNA = 0U,
    CAN_TCMAPPING_CUSTOM,
    CAN_TCMAPPING_MAP_100NM,
    CAN_TCMAPPING_MAP_150NM,
} CAN_tcMapping_E;

typedef enum
{
    CAN_APPSSTATE_SNA = 0U,
} CAN_appsState_E;

typedef enum
{
    CAN_BPPCSTATE_SNA = 0U,
} CAN_bppcState_E;

typedef enum
{
    CAN_SLEEPFOLLOWERSTATE_SNA = 0U,
} CAN_sleepFollowerState_E;

typedef enum
{
    FM_FAULT_VCFRONT_GEARCHANGEREJECTED = 0U,
    FM_FAULT_VCFRONT_LAUNCHREJECTED,
    FM_FAULT_COUNT,
} FM_fault_E;

typedef enum
{
    VCREAR_faults = 0U,
} CAN_message_E;

#define FM_FAULT_VCREAR_MCCALIBRATINGRESOLVER    0U

typedef struct
{
    uint16_t u16[4U];
} CANRX_rawMessage_S;

CANRX_MESSAGE_health_E CANRX_get_signal(CAN_bus_E bus, CAN_signal_E signal, void* value);
CANRX_MESSAGE_health_E CANRX_get_signal_digitalSna(CAN_digitalStatus_E* status);
CANRX_rawMessage_S*    CANRX_get_rawMessage(CAN_bus_E bus, CAN_message_E message);

#define CANRX_get_signal_func(bus, signal)    CANRX_get_signal_digitalSna
