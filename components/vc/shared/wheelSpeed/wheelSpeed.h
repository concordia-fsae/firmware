/**
 * @file wheelSpeed.h
 * @brief  Header file for wheel speed sensors
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CANTypes_generated.h"
#include "HW_tim.h"
#include "LIB_Types.h"
#include "ModuleDesc.h"
#include "wheel.h"
#include "MessageUnpack_generated.h"
#include <math.h>

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    enum
    {
        WS_SENSORTYPE_UNKNOWN = 0x00U,
        WS_SENSORTYPE_TIM_CHANNEL,
        WS_SENSORTYPE_CAN_RPM,
        WS_SENSORTYPE_CAN_MPS,
    } sensorType[WHEEL_CNT];
    union
    {
        HW_TIM_channelFreq_E channel_freq;
        CANRX_MESSAGE_health_E (*rpm)(float32_t* rpm);
        CANRX_MESSAGE_health_E (*mps)(float32_t* mps);
    } config[WHEEL_CNT];
} wheelSpeed_config_E;

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern const wheelSpeed_config_E wheelSpeed_config;
extern const ModuleDesc_S        wheelSpeed_desc;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

uint16_t  wheelSpeed_getSpeedRotational(wheel_E wheel);
float32_t wheelSpeed_getSpeedLinear(wheel_E wheel);
float32_t wheelSpeed_getSlipRatio(void);
