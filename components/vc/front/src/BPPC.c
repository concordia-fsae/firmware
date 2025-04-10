/**
* @file BPPC.c
* @brief Module source for the Brake Pedal Plausibility Check
*/

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "BPPC.h"
#include "APPS.h"
#include "drv_pedalMonitor.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "string.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    BPPC_state_E state;
} BPPC_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

BPPC_state_E BPPC_getState(void)
{
    return BPPC_data.state;
}

CAN_bppcState_E BPPC_getStateCAN(void)
{
    CAN_bppcState_E ret = CAN_BPPCSTATE_SNA;

    switch (BPPC_data.state)
    {
        case BPPC_OK:
            ret = CAN_BPPCSTATE_OK;
            break;
        case BPPC_INPLAUSIBLE:
            ret = CAN_BPPCSTATE_INPLAUSIBLE;
            break;
        case BPPC_FAULT:
            ret = CAN_BPPCSTATE_FAULT;
            break;
        case BPPC_ERROR:
            ret = CAN_BPPCSTATE_ERROR;
            break;
        default:
            break;
    }

    return ret;
}

static void BPPC_init(void)
{
    memset(&BPPC_data, 0x00U, sizeof(BPPC_data));
}

static void BPPC_periodic_100Hz(void)
{
    BPPC_state_E state = BPPC_ERROR;

    if (drv_pedalMonitor_getPedalState(DRV_PEDALMONITOR_BRAKE_POT) == DRV_PEDALMONITOR_OK)
    {
        const float32_t brake_pos = drv_pedalMonitor_getPedalPosition(DRV_PEDALMONITOR_BRAKE_POT);
        const float32_t accelerator_pos = APPS_getPedalPosition();

        if ((accelerator_pos > 0.10f) && (brake_pos > 0.25f))
        {
            state = BPPC_OK;
        }
        {
            state = BPPC_INPLAUSIBLE;
        }
    }
    else
    {
        state = BPPC_FAULT;
    }

    BPPC_data.state = state;
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S BPPC_desc = {
    .moduleInit = &BPPC_init,
    .periodic100Hz_CLK = &BPPC_periodic_100Hz,
};
