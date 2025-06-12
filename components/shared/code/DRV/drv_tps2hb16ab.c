/**
 * @file drv_tps2hb16ab.h
 * @brief Source file for the TPS2HB16A/B high side drivers
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_tps2hb16ab.h"
#include "drv_hsd.h"
#include "drv_io.h"
#include "string.h"
#include "HW_adc.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

struct
{
    float32_t         chip_temp;
    bool              request_enabled[DRV_TPS2HB16AB_IC_COUNT][DRV_TPS2HB16AB_OUT_COUNT];
    drv_hsd_state_E   state[DRV_TPS2HB16AB_IC_COUNT][DRV_TPS2HB16AB_OUT_COUNT];
    float32_t         current[DRV_TPS2HB16AB_IC_COUNT][DRV_TPS2HB16AB_OUT_COUNT];
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
            drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[i].enable[n], DRV_IO_INACTIVE);
        }
        drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[i].latch, DRV_IO_INACTIVE);
        drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[i].sel1, DRV_IO_INACTIVE);
        drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[i].sel2, DRV_IO_INACTIVE);
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
            switch (drv_tps2hb16ab_data.state[i][n])
            {
                // TODO: Handle state transitions
                case DRV_HSD_STATE_OFF:
                    if (drv_tps2hb16ab_data.request_enabled[i][n])
                    {
                        drv_tps2hb16ab_data.state[i][n] = DRV_HSD_STATE_ON;
                    }
                    break;
                case DRV_HSD_STATE_ON:
                    if (drv_tps2hb16ab_data.request_enabled[i][n])
                    {
                        if (drv_tps2hb16ab_data.current[i][n] > ADC_REF_VOLTAGE * 0.9f * drv_tps2hb16ab_ics[i].cs_amp_per_volt)
                        {
                            drv_tps2hb16ab_data.state[i][n] = DRV_HSD_STATE_OVERCURRENT;
                        }
                    }
                    else
                    {
                        drv_tps2hb16ab_data.state[i][n] = DRV_HSD_STATE_OFF;
                    }
                    break;
                default:
                    // FIXME: This should only occur if it is safe to reset
                    if (drv_tps2hb16ab_data.request_enabled[i][n] == false)
                    {
                        drv_tps2hb16ab_data.state[i][n] = DRV_HSD_STATE_OFF;
                    }
                    break;
            }

            if (drv_tps2hb16ab_data.state[i][n] == DRV_HSD_STATE_ON)
            {
                drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[i].enable[n], DRV_IO_ACTIVE);
            }
            else
            {
                drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[i].enable[n], DRV_IO_INACTIVE);
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
    const drv_io_activeState_E sel2_state = output == DRV_TPS2HB16AB_OUT_1 ? DRV_IO_INACTIVE : DRV_IO_INACTIVE;
    drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[ic].sel1, DRV_IO_INACTIVE);
    drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[ic].sel2, sel2_state);
}

void drv_tps2hb16ab_setDiagEnabled(drv_tps2hb16ab_E ic, bool enabled)
{
    const drv_io_activeState_E diag_state = enabled ? DRV_IO_INACTIVE : DRV_IO_INACTIVE;
    drv_outputAD_setDigitalActiveState(drv_tps2hb16ab_ics[ic].diag_en, diag_state);
}
