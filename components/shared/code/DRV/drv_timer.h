/**
 * @file drv_timer.h
 * @brief  Header file for the timer library
 * @note To use this driver, declare and initialize a timer. The timer can be
 * started, stopped, and evaluated with the different utility functions in this
 * driver.
 *
 * Setup
 * 1. Declare a timer
 * 2. Initialize the timer
 *
 * Usage
 * - A timer can be stopped, running, or expired. An expired timer will still tick
 *   and increment the elapsed time, however a stopped timer will always return an
 *   elapsed time of 0
 * - To run the timer for some amount of time, start the timer with the amount of
 *   time until the timer should be expired.
 * - To count the amount of time from a given point, a timer can be started with
 *   an arbitrary runtime. It will expire at that arbitrary amount of time, however
 *   the timer will still increase the elapsed time. It is suggested to use a
 *   runtime_ms of 0.
 * - Retrieve the state of a timer with the getState function
 * - Retrieve the amount of time in ms since the timer was started with the
 *   getElapsedTimeMs function
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stdint.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef uint32_t time_t;

typedef enum
{
    DRV_TIMER_STOPPED = 0x00U,
    DRV_TIMER_RUNNING,
    DRV_TIMER_EXPIRED,
} drv_timer_state_E;

typedef struct
{
    time_t            time_start_ms;
    time_t            runtime_ms;
    drv_timer_state_E state;
} drv_timer_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void              drv_timer_init(drv_timer_S * timer);
void              drv_timer_start(drv_timer_S * timer, time_t runtime_ms);
void              drv_timer_stop(drv_timer_S * timer);
drv_timer_state_E drv_timer_getState(drv_timer_S * timer);
time_t            drv_timer_getElapsedTimeMs(drv_timer_S *timer);
time_t            drv_timer_getEndTimeMS(drv_timer_S *timer);
