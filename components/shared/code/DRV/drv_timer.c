/**
 * @file drv_timer.c
 * @brief  Source file for the timer library
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_timer.h"
#include "HW_tim.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void drv_timer_init(drv_timer_S * timer)
{
    timer->time_start_ms = 0U;
    timer->runtime_ms = 0U;
    timer->state = DRV_TIMER_STOPPED;
}

void drv_timer_start(drv_timer_S * timer, time_t runtime_ms)
{
    timer->time_start_ms = HW_TIM_getTimeMS();
    timer->runtime_ms = runtime_ms;
    timer->state = DRV_TIMER_RUNNING;
}

void drv_timer_stop(drv_timer_S * timer)
{
    timer->state = DRV_TIMER_STOPPED;
}

drv_timer_state_E drv_timer_getState(drv_timer_S * timer)
{
    if ((timer->state == DRV_TIMER_RUNNING) &&
        (timer->runtime_ms < drv_timer_getElapsedTimeMs(timer)))
    {
        timer->state = DRV_TIMER_EXPIRED;
    }

    return timer->state;
}

time_t drv_timer_getElapsedTimeMs(drv_timer_S *timer)
{
    time_t delta = 0U;

    if (timer->state != DRV_TIMER_STOPPED)
    {
        delta = HW_TIM_getTimeMS() - timer->time_start_ms;
    }

    return delta;
}
