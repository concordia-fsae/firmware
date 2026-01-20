/**
 * @file drv_asm330.h
 * @brief Header file for the ST ASM330 line of IMU devices
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t accelX;
    float32_t accelY;
    float32_t accelZ;
} drv_imu_accel_S;

typedef struct
{
    float32_t rotX;
    float32_t rotY;
    float32_t rotZ;
} drv_imu_gyro_S;

typedef struct
{
    float32_t x;
    float32_t y;
    float32_t z;
} drv_imu_vector_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

