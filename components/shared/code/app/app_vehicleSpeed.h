/**
 * @file app_vehicleSpeed.h
 * @brief  Header file for wheel speed sensors
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"
#include "ModuleDesc.h"
#include "CANTypes_generated.h"
#include "HW_tim.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    WHEEL_FL = 0x00,
    WHEEL_FR,
    WHEEL_RL,
    WHEEL_RR,
    WHEEL_CNT,
} wheel_E;

typedef enum
{
    AXLE_FRONT = 0x00,
    AXLE_REAR,
    AXLE_CNT,
} axle_E;

typedef struct
{
    enum
    {
        WS_SENSORTYPE_UNKNOWN = 0x00U,
        WS_SENSORTYPE_TIM_CHANNEL,
        WS_SENSORTYPE_CAN_RPM,
    } sensorType[WHEEL_CNT];
    union
    {
        HW_TIM_channelFreq_E channel_freq;
        CANRX_MESSAGE_health_E (*rpm)(uint16_t* rpm);
    } config[WHEEL_CNT];
} app_wheelSpeed_config_S;

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern const app_wheelSpeed_config_S app_wheelSpeed_config;
extern const ModuleDesc_S app_vehicleSpeed_desc;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

uint16_t  app_vehicleSpeed_getAxleSpeedRotational(axle_E axle);
uint16_t  app_vehicleSpeed_getWheelSpeedRotational(wheel_E wheel);
float32_t app_vehicleSpeed_getWheelSpeedLinear(wheel_E wheel);
float32_t app_vehicleSpeed_getVehicleSpeed(void);
float32_t app_vehicleSpeed_getTireSlip(wheel_E wheel);
float32_t app_vehicleSpeed_getAxleSlip(axle_E axle);
