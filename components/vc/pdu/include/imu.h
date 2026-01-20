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
drv_imu_accel_S* imu_getAccelRef(void);
drv_imu_gyro_S* imu_getGyroRef(void);
