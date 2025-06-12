/**
* @file bppc.c
* @brief Module source for the Brake Pedal Plausibility Check
*/

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "bppc.h"
#include "apps.h"
#include "drv_pedalMonitor.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "string.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(FEATURE_BRAKEPEDAL_FROM_PRESSURE)
#define BRAKE_CHANNEL DRV_PEDALMONITOR_BRAKE_PR
#else
#define BRAKE_CHANNEL DRV_PEDALMONITOR_BRAKE_POT
#endif

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    bppc_state_E state;
    float32_t position;
} bppc_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t bppc_getPedalPosition(void)
{
    return bppc_data.position;
}

bppc_state_E bppc_getState(void)
{
    return bppc_data.state;
}

CAN_bppcState_E bppc_getStateCAN(void)
{
    CAN_bppcState_E ret = CAN_BPPCSTATE_SNA;

    switch (bppc_data.state)
    {
        case BPPC_OK:
            ret = CAN_BPPCSTATE_OK;
            break;
        case BPPC_FAULT:
            ret = CAN_BPPCSTATE_FAULT;
            break;
        case BPPC_FAULT_LATCHED:
            ret = CAN_BPPCSTATE_FAULT_LATCHED;
            break;
        case BPPC_ERROR:
            ret = CAN_BPPCSTATE_ERROR;
            break;
        default:
            break;
    }

    return ret;
}

static void bppc_init(void)
{
    memset(&bppc_data, 0x00U, sizeof(bppc_data));
}

static void bppc_periodic_100Hz(void)
{
    bppc_state_E state = BPPC_ERROR;

    if (drv_pedalMonitor_getPedalState(BRAKE_CHANNEL) == DRV_PEDALMONITOR_OK)
    {
        const float32_t brake_pos = drv_pedalMonitor_getPedalPosition(BRAKE_CHANNEL);
        const float32_t accelerator_pos = apps_getPedalPosition();
        bppc_data.position = brake_pos;

        if ((accelerator_pos > 0.25f) && (brake_pos > 0.10f))
        {
            state = BPPC_FAULT;
        }
        else if ((bppc_data.state == BPPC_FAULT) && (brake_pos <= 0.10f))
        {
            state = BPPC_FAULT_LATCHED;
        }
        else if ((accelerator_pos < 0.05f) || (bppc_data.state == BPPC_OK))
        {
            state = BPPC_OK;
        }
    }

    bppc_data.state = state;
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S bppc_desc = {
    .moduleInit = &bppc_init,
    .periodic100Hz_CLK = &bppc_periodic_100Hz,
};
