/**
 * lib_pid.c
 * Source code for the PID library
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_pid.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void lib_pid_init(lib_pid_S* pid, float32_t x_1, float32_t y_previous,
                  float32_t kp, float32_t ki, float32_t kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;

    pid->x = x_1;
    pid->x_1 = x_1;

    pid->p_term = 0.0f;
    pid->i_term = 0.0f;
    pid->d_term = 0.0f;

    pid->y = y_previous;
}

void lib_pi_typeb_calc(lib_pid_S* pid, float32_t setpoint, float32_t measure, float32_t dt)
{
    pid->x_1 = pid->x;
    pid->x = measure - setpoint;

    pid->p_term = pid->kp * pid->x;
    pid->i_term += pid->ki * pid->x * dt;
}

void lib_pid_typeb_calc(lib_pid_S* pid, float32_t setpoint, float32_t measure, float32_t dt)
{
    lib_pi_typeb_calc(pid, setpoint, measure, dt);
    pid->d_term += pid->kd * (pid->x / dt);
}

void lib_pid_typeb_sum(lib_pid_S* pid, float32_t out_min, float32_t out_max)
{
    float32_t res = pid->p_term + pid->i_term + pid->d_term;

    if (res < out_min)
    {
        pid->y = out_min;
    }
    else if (res > out_max)
    {
        pid->y = out_max;
    }
    else
    {
        pid->y = res;
    }
}

void lib_pid_util_ilim(lib_pid_S* pid, float i_min, float i_max)
{
    if (pid->i_term > i_max)
    {
        pid->i_term = i_max;
    }
    else if (pid->i_term < i_min)
    {
        pid->i_term = i_min;
    }
}
