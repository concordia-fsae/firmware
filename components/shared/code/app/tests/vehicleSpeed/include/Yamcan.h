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
    VCFRONT_vehicleSpeed = 0U,
} CAN_signal_E;

typedef enum
{
    FM_FAULT_COUNT = 0U,
} FM_fault_E;

typedef enum
{
    CAN_GPSQUALITYINDICATOR_SNA = 0U,
} CAN_gpsQualityIndicator_E;

typedef enum
{
    CAN_SLEEPFOLLOWERSTATE_SNA = 0U,
} CAN_sleepFollowerState_E;

CANRX_MESSAGE_health_E CANRX_get_signal(CAN_bus_E bus, CAN_signal_E signal, void* value);
