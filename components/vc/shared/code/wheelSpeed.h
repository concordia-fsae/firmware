/**
 * @file wheelSpeed.h
 * @brief  Header file for wheel speed sensors
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"
#include "ModuleDesc.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern const ModuleDesc_S wheelSpeed_desc;

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    WHEELSPEED_SENSOR_L = 0x00,
    WHEELSPEED_SENSOR_R,
    WHEELSPEED_SENSOR_CNT,
} wheelSpeed_Sensor_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

uint16_t  wheelSpeed_getSpeedRotational(wheelSpeed_Sensor_E wheel);
float32_t wheelSpeed_getSpeedLinear(wheelSpeed_Sensor_E wheel);
