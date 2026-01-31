/**
 * @file lib_madgwick.h
 * @brief Header file for the madgwick filter library
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include <stdint.h>
#include "drv_imu.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct {
    float q0, q1, q2, q3;
    float beta;
} lib_madgwick_S;

typedef drv_imu_euler_S lib_madgwick_euler_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * @brief Initialize filter.
 * @param f Pointer to filter.
 * @param beta Algorithm gain. Typical: 0.02..0.2 (higher = faster correction, noisier).
 */
void madgwick_init(lib_madgwick_S* f, float beta);

/**
 * @brief Set quaternion explicitly (will be normalized).
 */
void madgwick_set_quaternion(lib_madgwick_S* f, float q0, float q1, float q2, float q3);

/**
 * @brief Initialize quaternion from an accelerometer sample (yaw assumed zero).
 */
void madgwick_init_quaternion_from_accel(lib_madgwick_S* f, const lib_madgwick_euler_S* a);

/**
 * @brief Get Euler angles (rad) from quaternion.
 */
void madgwick_get_euler_rad(const lib_madgwick_S* f, lib_madgwick_euler_S* e);

/**
 * @brief Get Euler angles (deg) from quaternion.
 */
void madgwick_get_euler_deg(const lib_madgwick_S* f, lib_madgwick_euler_S* e);

/**
 * @brief Convert Euler angles from degrees to radians (in place).
 */
void madgwick_euler_deg_to_rad(lib_madgwick_euler_S* e);

/**
 * @brief Convert Euler angles from radians to degrees (in place).
 */
void madgwick_euler_rad_to_deg(lib_madgwick_euler_S* e);

/**
 * @brief Update using gyro (deg/s) and accel (g or m/s^2; only direction matters).
 * @param dt Seconds since last update.
 */
void madgwick_update_imu(lib_madgwick_S* f,
                         const lib_madgwick_euler_S* g,
                         const lib_madgwick_euler_S* a,
                         float dt);

/**
 * @brief Update using gyro (deg/s), accel, and magnetometer (any units; only direction matters).
 * @param dt Seconds since last update.
 */
void madgwick_update_mag(lib_madgwick_S* f,
                         const lib_madgwick_euler_S* g,
                         const lib_madgwick_euler_S* a,
                         const lib_madgwick_euler_S* m,
                         float dt);
