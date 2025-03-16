/**
 * @file drv_timer.h
 * @brief  Header file for the timer library
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
