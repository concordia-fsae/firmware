/**
 * @file imu.h
 * @brief Public API for IMU bring-up and sensor data access
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/
#include "LIB_Types.h"     

/******************************************************************************
 *                      T Y P E D E F S   /   E N U M S
 ******************************************************************************/

typedef enum {
    IMU_STATUS_OK = 0U,
    IMU_STATUS_ERROR = 1U,
    IMU_STATUS_NOT_FOUND = 2U,
    IMU_STATUS_BUSY = 3U,
} imu_status_e;

typedef struct {
    float ax_ms2;
    float ay_ms2;
    float az_ms2;
    float gx_rads;
    float gy_rads;
    float gz_rads;
    float temp_c;
} imu_siData_s;

/******************************************************************************
 *            P U B L I C   F U N C T I O N   P R O T O T Y P E S
 ******************************************************************************/

/**
 * @brief Initialize the IMU over SPI and configure basic settings
 * @return IMU_STATUS_OK if device responds and config applied
 */
imu_status_e imu_init(void);

/**
 * @brief Read WHO_AM_I register for sanity check
 * @param[out] id register value
 * @return IMU_STATUS_OK if read was successful
 */
imu_status_e imuReadWhoAmI(uint8_t *id);

/**
 * @brief Poll sensor registers and update SI data
 * @param[out] data structure with accel/gyro/temp in SI units
 * @return IMU_STATUS_OK if data read correctly
 */
imu_status_e imuReadData(imu_siData_s *data);