/**
* @file APPS.c
* @brief Module for the Accelerator Pedal Position Sensor
 * @note Pedal positon is a float percentage between 0.0f and 1.0f where
 *       0.0f is 0% and 1.0f is 100%
*/

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

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
    float32_t pedal_position; // [%] 0.0f - 1.0f | 0.0f = 0% ad 1.0f = 100%
    APPS_state_E state;
} APPS_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t APPS_getPedalPosition(void)
{
    return (APPS_data.state == APPS_OK) ? APPS_data.pedal_position : 0.0f;
}

APPS_state_E APPS_getState(void)
{
    return APPS_data.state;
}

CAN_appsState_E APPS_getStateCAN(void)
{
    CAN_appsState_E ret = CAN_APPSSTATE_SNA;

    switch (APPS_data.state)
    {
        case APPS_OK:
            ret = CAN_APPSSTATE_OK;
            break;
        case APPS_FAULT_P1:
            ret = CAN_APPSSTATE_FAULT_PEDAL1;
            break;
        case APPS_FAULT_P2:
            ret = CAN_APPSSTATE_FAULT_PEDAL2;
            break;
        case APPS_FAULT_BOTH:
            ret = CAN_APPSSTATE_FAULT_BOTH;
            break;
        case APPS_FAULT_DISAGREEMENT:
            ret = CAN_APPSSTATE_FAULT_DISAGREEMENT;
            break;
        case APPS_ERROR:
            ret = CAN_APPSSTATE_ERROR;
            break;
        default:
            break;
    }

    return ret;
}

static void APPS_init(void)
{
    memset(&APPS_data, 0x00U, sizeof(APPS_data));
}

static void APPS_periodic_100Hz(void)
{
    // Reset the pedal position unless set otherwise by the periodic
    float32_t pedal_position = 0.0f;
    APPS_state_E state = APPS_ERROR;

    if ((drv_pedalMonitor_getPedalState(DRV_PEDALMONITOR_APPS1) != DRV_PEDALMONITOR_OK) &&
        (drv_pedalMonitor_getPedalState(DRV_PEDALMONITOR_APPS2) != DRV_PEDALMONITOR_OK))
    {
        state = APPS_FAULT_BOTH;
    }
    else if (drv_pedalMonitor_getPedalState(DRV_PEDALMONITOR_APPS1) != DRV_PEDALMONITOR_OK)
    {
        state = APPS_FAULT_P1;
    }
    else if (drv_pedalMonitor_getPedalState(DRV_PEDALMONITOR_APPS2) != DRV_PEDALMONITOR_OK)
    {
        state = APPS_FAULT_P2;
    }
    else
    {
        const float32_t apps1 = drv_pedalMonitor_getPedalPosition(DRV_PEDALMONITOR_APPS1);
        const float32_t apps2 = drv_pedalMonitor_getPedalPosition(DRV_PEDALMONITOR_APPS2);
        const float32_t apps_difference = apps1 - apps2;

        if ((apps_difference > 0.10f)  || (apps_difference < -0.10f))
        {
            state = APPS_FAULT_DISAGREEMENT;
        }
        else
        {
            const float32_t apps_average = (apps1 + apps2) / 2.0f;

            pedal_position = apps_average;
            state = APPS_OK;
        }
    }

    APPS_data.pedal_position = pedal_position;
    APPS_data.state = state;
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S APPS_desc = {
    .moduleInit = &APPS_init,
    .periodic100Hz_CLK = &APPS_periodic_100Hz,
};
