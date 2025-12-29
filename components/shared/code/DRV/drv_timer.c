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

/**
 * @brief Initialize a timer
 * @param timer The timer to act upon
 */
void drv_timer_init(drv_timer_S * timer)
{
    timer->time_start_ms = 0U;
    timer->runtime_ms = 0U;
    timer->state = DRV_TIMER_STOPPED;
}

/**
 * @brief Start a timer for the provided amount of time
 * @param timer The timer to act upon
 * @param runtime_ms The amount of time in ms the timer shall run before expiring.
 */
void drv_timer_start(drv_timer_S * timer, time_t runtime_ms)
{
    timer->time_start_ms = HW_TIM_getTimeMS();
    timer->runtime_ms = runtime_ms;
    timer->state = DRV_TIMER_RUNNING;
}

/**
 * @brief Stop a timer
 * @param timer The timer to act upon
 */
void drv_timer_stop(drv_timer_S * timer)
{
    timer->state = DRV_TIMER_STOPPED;
}

/**
 * @brief Get the state of the timer
 * @param timer The timer to retrieve the state of
 * @return the active state of the timer
 */
drv_timer_state_E drv_timer_getState(drv_timer_S * timer)
{
    if ((timer->state == DRV_TIMER_RUNNING) &&
        (timer->runtime_ms <= drv_timer_getElapsedTimeMs(timer)))
    {
        timer->state = DRV_TIMER_EXPIRED;
    }

    return timer->state;
}

/**
 * @brief Get the elapsed time in ms of the timer
 * @note If a timer is stopped, it will report an elapsed time of 0
 * @param timer The timer to get the elapsed time of
 * @return The time in ms since the timer was stopped.
 */
time_t drv_timer_getElapsedTimeMs(drv_timer_S *timer)
{
    time_t delta = 0U;

    if (timer->state != DRV_TIMER_STOPPED)
    {
        delta = HW_TIM_getTimeMS() - timer->time_start_ms;
    }

    return delta;
}

/**
 * @brief Get the end time of a timer
 * @param timer The timer to get the elapsed time of
 * @return The time in ms at which point the timer will expire
 */
time_t drv_timer_getEndTimeMS(drv_timer_S *timer)
{
    return timer->time_start_ms + timer->runtime_ms;
}

