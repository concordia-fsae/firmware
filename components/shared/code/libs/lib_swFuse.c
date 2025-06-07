/**
 * @file drv_hsd.c
 * @brief Source file for software fuxe functionalities
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_swFuse.h"
#include "HW_tim.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void lib_swFuse_init(lib_swFuse_fuse_S* fuse)
{
    fuse->state = LIB_SWFUSE_OK;
    fuse->current_i2t = 0.0f;
    fuse->last_run_ms = HW_TIM_getTimeMS();
    drv_timer_init(&fuse->cooldown_timer);
}

lib_swFuse_state_E lib_swFuse_runCurrent(lib_swFuse_fuse_S* fuse, float32_t current)
{
    const uint32_t current_time = HW_TIM_getTimeMS();
    const float32_t delta_s = ((float32_t)(current_time - fuse->last_run_ms)) / 1000.0f;
    fuse->last_run_ms = current_time;

    switch (fuse->state)
    {
        case LIB_SWFUSE_OK:
            {
                const bool overcurrent = current > fuse->config.overcurrent_threshold;
                const float32_t run_increment = overcurrent ? current * current * delta_s : 0.0f;

                if (overcurrent)
                {
                    fuse->current_i2t += run_increment;

                    if (fuse->current_i2t > fuse->config.max_i2t)
                    {
                        drv_timer_start(&fuse->cooldown_timer, fuse->config.over_energy_cooldown_ms);
                        fuse->state = LIB_SWFUSE_OVERCURRENT;
                    }
                }
                else
                {
                    fuse->current_i2t = 0.0f;
                }
            }
            break;
        case LIB_SWFUSE_OVERCURRENT:
            if (drv_timer_getState(&fuse->cooldown_timer) == DRV_TIMER_EXPIRED)
            {
                fuse->state = LIB_SWFUSE_OK;
                drv_timer_stop(&fuse->cooldown_timer);
            }
            break;
        default:
            break;
    }

    return fuse->state;
}

lib_swFuse_state_E lib_swFuse_getState(lib_swFuse_fuse_S *fuse)
{
    return fuse->state;
}

float32_t lib_swFuse_geti2t(lib_swFuse_fuse_S *fuse)
{
    return fuse->current_i2t;
}
