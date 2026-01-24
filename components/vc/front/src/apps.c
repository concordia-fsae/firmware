/**
* @file apps.c
* @brief Module for the Accelerator Pedal Position Sensor
 * @note Pedal positon is a float percentage between 0.0f and 1.0f where
 *       0.0f is 0% and 1.0f is 100%
*/

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "apps.h"
#include "drv_pedalMonitor.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "string.h"
#include "lib_utility.h"
#include "app_faultManager.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(FEATURE_WIGGLY_PEDAL)
#define PEDAL_TOLERANCE 0.20f
#else
#define PEDAL_TOLERANCE 0.10f
#endif

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    float32_t pedal_position; // [%] 0.0f - 1.0f | 0.0f = 0% ad 1.0f = 100%
    apps_state_E state;
} apps_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t apps_getPedalPosition(void)
{
    return (apps_data.state == APPS_OK) ? apps_data.pedal_position : 0.0f;
}

apps_state_E apps_getState(void)
{
    return apps_data.state;
}

CAN_appsState_E apps_getStateCAN(void)
{
    CAN_appsState_E ret = CAN_APPSSTATE_SNA;

    switch (apps_data.state)
    {
        case APPS_OK:
            ret = CAN_APPSSTATE_OK;
            break;
        case APPS_DISAGREEMENT:
            ret = CAN_APPSSTATE_DISAGREEMENT;
            break;
        case APPS_ERROR:
            ret = CAN_APPSSTATE_ERROR;
            break;
        default:
            break;
    }

    return ret;
}

static void apps_init(void)
{
    memset(&apps_data, 0x00U, sizeof(apps_data));
}

static void apps_periodic_100Hz(void)
{
    const bool okApps1 = drv_pedalMonitor_getPedalState(DRV_PEDALMONITOR_APPS1) == DRV_PEDALMONITOR_OK;
    const bool okApps2 = drv_pedalMonitor_getPedalState(DRV_PEDALMONITOR_APPS2) == DRV_PEDALMONITOR_OK;

    // Reset the pedal position unless set otherwise by the periodic
    float32_t pedal_position = 0.0f;
    apps_state_E state = APPS_ERROR;

    if (okApps1 && okApps2)
    {
        const float32_t apps1 = drv_pedalMonitor_getPedalPosition(DRV_PEDALMONITOR_APPS1);
        const float32_t apps2 = drv_pedalMonitor_getPedalPosition(DRV_PEDALMONITOR_APPS2);
        const float32_t apps_difference = apps1 - apps2;

        if ((apps_difference > PEDAL_TOLERANCE)  || (apps_difference < -PEDAL_TOLERANCE))
        {
            state = APPS_DISAGREEMENT;
        }
        else
        {
            const float32_t apps_average = (apps1 + apps2) / 2.0f;

            pedal_position = apps_average;
            state = APPS_OK;
        }
    }

    apps_data.pedal_position = SATURATE(0.0f, pedal_position, 1.0f);
    apps_data.state = state;
    app_faultManager_setFaultState(FM_FAULT_VCFRONT_APPS1SENSORFAULT, !okApps1);
    app_faultManager_setFaultState(FM_FAULT_VCFRONT_APPS2SENSORFAULT, !okApps2);
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S apps_desc = {
    .moduleInit = &apps_init,
    .periodic100Hz_CLK = &apps_periodic_100Hz,
};
