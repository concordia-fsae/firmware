/**
 * @file drv_hsd.h
 * @brief Generalized header file for all common high side driver functions
 */

#pragma once

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_HSD_STATE_INIT = 0x00U,
    DRV_HSD_STATE_OFF,
    DRV_HSD_STATE_ON,
    DRV_HSD_STATE_SW_FUSED,
    DRV_HSD_STATE_OVERCURRENT,
    DRV_HSD_STATE_OVERTEMP,
    DRV_HSD_STATE_RETRY,
    DRV_HSD_STATE_ERROR,
} drv_hsd_state_E;
