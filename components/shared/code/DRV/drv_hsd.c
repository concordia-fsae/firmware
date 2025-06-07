/**
 * @file drv_hsd.c
 * @brief Generalized source file for all common high side driver functions
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_hsd.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

CAN_hsdState_E drv_hsd_getCANState(drv_hsd_state_E state)
{
    switch (state)
    {
        case DRV_HSD_STATE_OFF:
            return CAN_HSDSTATE_OFF;
        case DRV_HSD_STATE_ON:
            return CAN_HSDSTATE_ON;
        case DRV_HSD_STATE_SW_FUSED:
            return CAN_HSDSTATE_SW_FUSED;
        case DRV_HSD_STATE_OVERCURRENT:
            return CAN_HSDSTATE_OVERCURRENT;
        case DRV_HSD_STATE_OVERTEMP:
            return CAN_HSDSTATE_OVERTEMP;
        case DRV_HSD_STATE_RETRY:
            return CAN_HSDSTATE_RETRY;
        case DRV_HSD_STATE_ERROR:
            return CAN_HSDSTATE_FAULT;
        case DRV_HSD_STATE_INIT:
        default:
            return CAN_HSDSTATE_SNA;
    }
}
