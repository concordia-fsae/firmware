/**
 * @file imu.h
 * @brief  Header file for imu Application
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_imu.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void imu_getAccel(drv_imu_accel_S* accel);
void imu_getGyro(drv_imu_gyro_S* accel);
void imu_getVehicleAngle(drv_imu_gyro_S* accel);

float32_t imu_getAccelNorm(void);
float32_t imu_getAccelNormPeak(void);
float32_t imu_getAngleFromGravity(void);

drv_imu_accel_S* imu_getAccelRef(void);
drv_imu_gyro_S* imu_getGyroRef(void);
drv_imu_gyro_S* imu_getVehicleAngleRef(void);

bool imu_getCrashEvent(void);
bool imu_getImpactActive(void);
float32_t imu_getImpactAccelCurrent(void);
float32_t imu_getImpactAccelMax(void);
bool imu_isFaulted(void);
