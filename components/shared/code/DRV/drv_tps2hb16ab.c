/**
 * @file drv_tps2hb16ab.h
 * @brief Source file for the TPS2HB16A/B high side drivers
 *
 * Initialization
 * 1. Configure each HSD's drv_tps2hb16ab_ic_S
 * 2. Initialize all of the HSDs with drv_tps2hb16ab_init
 * 3. The user may choose to do open circuit/short circuit diagnostics
 *    on first power up or at any time by disabling the HSD channel, enabling
 *    diagnostics, and evaluating if the SNS line is held high for a given channel
 *
 * Usage
 * 1. Enable or disable the HSD channel depending on the application needs
 * 2. Enable fault latching if the application requires the HSD to immediately trip
 *    on a fault condition, otherwise disable fault latching
 * 3. If a fault is detected by the HSD, the HSD will transition into OVERCURRENT.
 *    Once in OVERCURRENT, the user must disable the faulted HSD channel. If the
 *    HSD stays faulted, then the channel is in an OVERTEMPERATURE condition, and
 *    the application must wait until the HSD cools down and exits the fault state
 *    before being re-enabled.
 * 4. The application must select which current sense channel to sense from, the
 *    HSD driver does not change the channel automatically
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_tps2hb16ab.h"
#include "drv_hsd.h"
#include "drv_io.h"
#include "string.h"
#include "HW_adc.h"
#include "drv_timer.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

struct
{
    float32_t         chip_temp;
    bool              request_enabled[DRV_TPS2HB16AB_IC_COUNT][DRV_TPS2HB16AB_OUT_COUNT];
    drv_hsd_state_E   state[DRV_TPS2HB16AB_IC_COUNT][DRV_TPS2HB16AB_OUT_COUNT];
    float32_t         current[DRV_TPS2HB16AB_IC_COUNT][DRV_TPS2HB16AB_OUT_COUNT];
    drv_timer_S       oc_timer[DRV_TPS2HB16AB_IC_COUNT][DRV_TPS2HB16AB_OUT_COUNT];
} drv_tps2hb16ab_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void drv_tps2hb16ab_init(void)
{
    memset(&drv_tps2hb16ab_data, 0x00U, sizeof(drv_tps2hb16ab_data));

    for (uint8_t i = 0U; i < DRV_TPS2HB16AB_IC_COUNT; i++)
    {
        for (uint8_t n = 0U; n < DRV_TPS2HB16AB_OUT_COUNT; n++)
        {
            drv_tps2hb16ab_data.state[i][n] = DRV_HSD_STATE_OFF;
            drv_tps2hb16ab_data.request_enabled[i][n] = false;
            drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[i].channel[n].enable, DRV_IO_INACTIVE);
            drv_timer_init(&drv_tps2hb16ab_data.oc_timer[i][n]);
        }
        drv_tps2hb16ab_setCSChannel(i, DRV_TPS2HB16AB_OUT_1);
        drv_tps2hb16ab_setDiagEnabled(i, false);
        drv_tps2hb16ab_setFaultLatch(i, true);
    }
}

void drv_tps2hb16ab_run(void)
{
    for (uint8_t i = 0U; i < DRV_TPS2HB16AB_IC_COUNT; i++)
    {
        const float32_t cs_voltage = drv_inputAD_getAnalogVoltage(drv_tps2hb16ab_ics[i].cs_channel);
        const bool diag = drv_outputAD_getDigitalActiveState(drv_tps2hb16ab_ics[i].diag_en) == DRV_IO_ACTIVE;

        if (diag)
        {
            const bool measuring_temp = drv_outputAD_getDigitalActiveState(drv_tps2hb16ab_ics[i].sel1) == DRV_IO_ACTIVE;
            const drv_tps2hb16ab_output_E selected_channel = drv_outputAD_getDigitalActiveState(drv_tps2hb16ab_ics[i].sel2) == DRV_IO_ACTIVE ?
                                                             DRV_TPS2HB16AB_OUT_2 :
                                                             DRV_TPS2HB16AB_OUT_1;

            if (measuring_temp)
            {
                // TODO: Implement
            }
            else
            {
                const float32_t current = cs_voltage * drv_tps2hb16ab_ics[i].cs_amp_per_volt;
                drv_tps2hb16ab_data.current[i][selected_channel] = current;
            }
        }

        for (uint8_t n = 0U; n < DRV_TPS2HB16AB_OUT_COUNT; n++)
        {
            // The overcurrent condition provided by the HSD is a high current measurement (over the max that the
            // circuit can handle). This condition on the current sense line indicates that a given channel is in
            // a faulted state. To determine the type of fault, it is required to have context of the state
            // progressions that the HSD undergoes.
            const bool is_overcurrent = drv_tps2hb16ab_data.current[i][n] > drv_tps2hb16ab_ics[i].channel[n].current_limit_amp;
            if (is_overcurrent)
            {
                const drv_timer_state_E channel_state = drv_timer_getState(&drv_tps2hb16ab_data.oc_timer[i][n]);
                if (channel_state == DRV_TIMER_STOPPED)
                {
                    drv_timer_start(&drv_tps2hb16ab_data.oc_timer[i][n], drv_tps2hb16ab_ics[i].channel[n].oc_timeout_ms);
                }
                else if (channel_state == DRV_TIMER_EXPIRED)
                {
                    drv_tps2hb16ab_data.state[i][n] = DRV_HSD_STATE_OVERCURRENT;
                    drv_tps2hb16ab_setFaultLatch(i, false);
                }
            }
            else
            {
                drv_timer_stop(&drv_tps2hb16ab_data.oc_timer[i][n]);
            }

            switch (drv_tps2hb16ab_data.state[i][n])
            {
                // Fault transitions of the TPS2HB16A/B
                // 1. If OFF and diagnostics is enabled, check for open circuit/short circuit
                // 1.1 If HSD output is OC/SC, then set SNS line of the faulted channel high
                // 2. If ON and over current/over temperature occurs, set SNS line of the faulted
                //    channel high
                // 2.1 SNS line goes low once the latch pin is low, enable goes low, and the fault
                //     condition goes away
                case DRV_HSD_STATE_OFF:
                    if (drv_tps2hb16ab_data.request_enabled[i][n])
                    {
                        drv_tps2hb16ab_data.state[i][n] = DRV_HSD_STATE_ON;
                    }
                    break;
                case DRV_HSD_STATE_ON:
                    if (drv_tps2hb16ab_data.request_enabled[i][n] == false)
                    {
                        drv_tps2hb16ab_data.state[i][n] = DRV_HSD_STATE_OFF;
                    }
                    break;
                case DRV_HSD_STATE_OVERCURRENT:
                    if (drv_tps2hb16ab_data.request_enabled[i][n] == false)
                    {
                        if (is_overcurrent)
                        {
                            drv_tps2hb16ab_data.state[i][n] = DRV_HSD_STATE_OVERTEMP;
                        }
                        else
                        {
                            drv_tps2hb16ab_data.state[i][n] = DRV_HSD_STATE_OFF;
                            drv_tps2hb16ab_setFaultLatch(i, true);
                        }
                    }
                    break;
                case DRV_HSD_STATE_OVERTEMP:
                    if ((is_overcurrent == false) && (drv_tps2hb16ab_data.request_enabled[i][n] == false))
                    {
                        drv_tps2hb16ab_data.state[i][n] = DRV_HSD_STATE_OFF;
                        drv_tps2hb16ab_setFaultLatch(i, true);
                    }
                    break;
                default:
                    if (drv_tps2hb16ab_data.request_enabled[i][n] == false)
                    {
                        drv_tps2hb16ab_data.state[i][n] = DRV_HSD_STATE_OFF;
                    }
                    break;
            }

            if (drv_tps2hb16ab_data.state[i][n] == DRV_HSD_STATE_ON)
            {
                drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[i].channel[n].enable, DRV_IO_ACTIVE);
            }
            else
            {
                drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[i].channel[n].enable, DRV_IO_INACTIVE);
            }
        }
    }
}

drv_hsd_state_E drv_tps2hb16ab_getState(drv_tps2hb16ab_E ic, drv_tps2hb16ab_output_E output)
{
    return drv_tps2hb16ab_data.state[ic][output];
}

float32_t drv_tps2hb16ab_getCurrent(drv_tps2hb16ab_E ic, drv_tps2hb16ab_output_E output)
{
    return drv_tps2hb16ab_data.current[ic][output];
}

void drv_tps2hb16ab_setEnabled(drv_tps2hb16ab_E ic, drv_tps2hb16ab_output_E output, bool enabled)
{
    drv_tps2hb16ab_data.request_enabled[ic][output] = enabled;
}

void drv_tps2hb16ab_setCSChannel(drv_tps2hb16ab_E ic, drv_tps2hb16ab_output_E output)
{
    const drv_io_activeState_E sel2_state = output == DRV_TPS2HB16AB_OUT_1 ? DRV_IO_INACTIVE : DRV_IO_ACTIVE;
    drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[ic].sel1, DRV_IO_INACTIVE);
    drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[ic].sel2, sel2_state);
}

void drv_tps2hb16ab_setDiagEnabled(drv_tps2hb16ab_E ic, bool enabled)
{
    const drv_io_activeState_E diag_state = enabled ? DRV_IO_ACTIVE : DRV_IO_INACTIVE;
    drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[ic].diag_en, diag_state);
}

void drv_tps2hb16ab_setFaultLatch(drv_tps2hb16ab_E ic, bool latch_on_fault)
{
    const drv_io_activeState_E latch_state = latch_on_fault ? DRV_IO_INACTIVE : DRV_IO_ACTIVE;
    drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[ic].latch, latch_state);
}
