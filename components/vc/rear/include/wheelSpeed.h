/**
 * @file wheelSpeed.h
 * @brief  Header file for wheel speed sensors
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define set_velocityL(m,b,n,s) set(m,b,n,s, wheelSpeed_getMps(WHEELSPEED_SENSOR_L))
#define set_velocityR(m,b,n,s) set(m,b,n,s, wheelSpeed_getMps(WHEELSPEED_SENSOR_R))
#define set_rpmL(m,b,n,s) set(m,b,n,s, wheelSpeed_getRpm(WHEELSPEED_SENSOR_L))
#define set_rpmR(m,b,n,s) set(m,b,n,s, wheelSpeed_getRpm(WHEELSPEED_SENSOR_R))

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

uint16_t  wheelSpeed_getRpm(wheelSpeed_Sensor_E wheel);
float32_t wheelSpeed_getMps(wheelSpeed_Sensor_E wheel);
