/**
 * @file lib_swFuse.h
 * @brief Header file for software fuse functionalities
 *
 * Setup
 * 1. Define the `lib_swFuse_fuse_S` configuration struct
 * 2. Call `lib_swFuse_init` on the struct
 *
 * Usage
 * - Every period of the calling algorithm, run the model with `lib_swFuse_runCurrent`
 * - Get the state of the software fuse with `lib_swFuse_getState`
 * - Get the current value of i^2*t with `lib_swFuse_geti2t`
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"
#include "drv_timer.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    LIB_SWFUSE_INIT = 0x00U,
    LIB_SWFUSE_OK,
    LIB_SWFUSE_OVERCURRENT,
} lib_swFuse_state_E;

typedef struct
{
    const struct
    {
        float32_t overcurrent_threshold;
        float32_t max_i2t;
        uint16_t  over_energy_cooldown_ms;
    } config;
    lib_swFuse_state_E state;
    float32_t          current_i2t;
    drv_timer_S        cooldown_timer;
    uint32_t           last_run_ms;
} lib_swFuse_fuse_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void               lib_swFuse_init(lib_swFuse_fuse_S* fuse);
lib_swFuse_state_E lib_swFuse_runCurrent(lib_swFuse_fuse_S* fuse, float32_t current);
lib_swFuse_state_E lib_swFuse_getState(lib_swFuse_fuse_S *fuse);
float32_t          lib_swFuse_geti2t(lib_swFuse_fuse_S *fuse);
