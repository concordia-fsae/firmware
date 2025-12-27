/**
 * @file drv_vn9008.c
 * @brief Source file for the VN9008 high side driver
 *
 * Setup
 *
 * Usage
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_io.h"
#include "drv_vn9008.h"
#include "HW_adc.h"
#include "string.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

struct
{
    bool            request_enabled[DRV_VN9008_CHANNEL_COUNT];
    drv_hsd_state_E state[DRV_VN9008_CHANNEL_COUNT];
    float32_t       current[DRV_VN9008_CHANNEL_COUNT];
    drv_timer_S     oc_timer[DRV_VN9008_CHANNEL_COUNT];
} drv_vn9008_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void drv_vn9008_init(void)
{
    memset(&drv_vn9008_data, 0x00U, sizeof(drv_vn9008_data));

    for (uint8_t i = 0U; i < DRV_VN9008_CHANNEL_COUNT; i++)
    {
        drv_vn9008_data.state[i]           = DRV_HSD_STATE_OFF;
        drv_vn9008_data.request_enabled[i] = false;
        drv_outputAD_setDigitalActiveState(drv_vn9008_channels[i].enable,      DRV_IO_INACTIVE);
        drv_outputAD_setDigitalActiveState(drv_vn9008_channels[i].enable_cs,   DRV_IO_INACTIVE);
        drv_outputAD_setDigitalActiveState(drv_vn9008_channels[i].fault_reset, DRV_IO_INACTIVE);
    }
}

void drv_vn9008_run(void)
{
    for (uint8_t i = 0U; i < DRV_VN9008_CHANNEL_COUNT; i++)
    {
        const float32_t cs_voltage     = drv_inputAD_getAnalogVoltage(drv_vn9008_channels[i].cs_channel);
        const float32_t current        = cs_voltage * drv_vn9008_channels[i].cs_amp_per_volt;
        const bool      diagnostic     = drv_outputAD_getDigitalActiveState(drv_vn9008_channels[i].enable_cs) == DRV_IO_ACTIVE;
        drv_vn9008_data.current[i]     = diagnostic ? current : drv_vn9008_data.current[i];
        const bool      is_overcurrent = drv_vn9008_data.current[i] > drv_vn9008_channels[i].current_limit_amp;

        if (is_overcurrent)
        {
            const drv_timer_state_E state = drv_timer_getState(&drv_vn9008_data.oc_timer[i]);
            if (state == DRV_TIMER_STOPPED)
            {
                drv_timer_start(&drv_vn9008_data.oc_timer[i], drv_vn9008_channels[i].oc_timeout_ms);
            }
        }
        else
        {
            drv_timer_stop(&drv_vn9008_data.oc_timer[i]);
        }

        switch (drv_vn9008_data.state[i])
        {
            case DRV_HSD_STATE_OFF:
                if (drv_vn9008_data.request_enabled[i])
                {
                    drv_outputAD_setDigitalActiveState(drv_vn9008_channels[i].fault_reset, DRV_IO_INACTIVE);
                    drv_vn9008_data.state[i] = DRV_HSD_STATE_ON;
                }
                break;

            case DRV_HSD_STATE_ON:
                const drv_timer_state_E state = drv_timer_getState(&drv_vn9008_data.oc_timer[i]);
                if (drv_vn9008_data.request_enabled[i] == false)
                {
                    drv_vn9008_data.state[i] = DRV_HSD_STATE_OFF;
                }
                else if (state == DRV_TIMER_EXPIRED)
                {
                    drv_vn9008_data.state[i] = DRV_HSD_STATE_OVERCURRENT;
                    drv_outputAD_setDigitalActiveState(drv_vn9008_channels[i].fault_reset, DRV_IO_ACTIVE);
                    drv_timer_stop(&drv_vn9008_data.oc_timer[i]);
                }
                break;

            case DRV_HSD_STATE_OVERCURRENT:
                if (drv_vn9008_data.request_enabled[i] == false)
                {
                    if (is_overcurrent)
                    {
                        drv_vn9008_data.state[i] = DRV_HSD_STATE_OVERTEMP;
                    }
                    else if (is_overcurrent == false)
                    {
                        drv_vn9008_data.state[i] = DRV_HSD_STATE_OFF;
                        drv_outputAD_setDigitalActiveState(drv_vn9008_channels[i].fault_reset, DRV_IO_INACTIVE);
                    }
                }
                break;

            case DRV_HSD_STATE_OVERTEMP:
                if (drv_vn9008_data.request_enabled[i] == false)
                {
                    if (is_overcurrent == false)
                    {
                        drv_vn9008_data.state[i] = DRV_HSD_STATE_OFF;
                        drv_outputAD_setDigitalActiveState(drv_vn9008_channels[i].fault_reset, DRV_IO_INACTIVE);
                    }
                }
                break;

            default:
                drv_vn9008_data.state[i] = DRV_HSD_STATE_OFF;
                break;
        }

        if (drv_vn9008_data.state[i] == DRV_HSD_STATE_ON)
        {
            drv_outputAD_setDigitalActiveState(drv_vn9008_channels[i].enable, DRV_IO_ACTIVE);
        }
        else
        {
            drv_outputAD_setDigitalActiveState(drv_vn9008_channels[i].enable, DRV_IO_INACTIVE);
        }
    }
}

drv_hsd_state_E drv_vn9008_getState(drv_vn9008_E ic)
{
    return drv_vn9008_data.state[ic];
}

float32_t drv_vn9008_getCurrent(drv_vn9008_E ic)
{
    return drv_vn9008_data.current[ic];
}

void drv_vn9008_setEnabled(drv_vn9008_E ic, bool enabled)
{
    drv_vn9008_data.request_enabled[ic] = enabled;
}

void drv_vn9008_setCSEnabled(drv_vn9008_E ic, bool enabled)
{
    const drv_io_activeState_E state = enabled ? DRV_IO_ACTIVE : DRV_IO_INACTIVE;

    drv_outputAD_setDigitalActiveState(drv_vn9008_channels[ic].enable_cs, state);
}
