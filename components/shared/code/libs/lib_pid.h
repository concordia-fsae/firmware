/**
 * lib_pid.h
 * Header file for different PID implementations
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
    float32_t kp;
    float32_t ki;
    float32_t kd;

    float32_t x;
    float32_t x_1;

    float32_t p_term;
    float32_t i_term;
    float32_t d_term;

    float32_t y;
} lib_pid_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void lib_pid_init(lib_pid_S* pid, float32_t x_1, float32_t y_previous,
                  float32_t kp, float32_t ki, float32_t kd);
void lib_pi_typeb_calc(lib_pid_S* pid, float32_t setpoint, float32_t measure, float32_t dt);
void lib_pid_typeb_calc(lib_pid_S* pid, float32_t setpoint, float32_t measure, float32_t dt);
void lib_pid_typeb_sum(lib_pid_S* pid, float32_t out_min, float32_t out_max);

void lib_pid_util_ilim(lib_pid_S* pid, float i_min, float i_max);
