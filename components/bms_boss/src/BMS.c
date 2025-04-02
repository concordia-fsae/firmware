/**
 * @file BMS.c
 * @brief  Source code for BMS manager
 */

#include "BMS.h"

#include <string.h>

#include "HW.h"
#include "HW_gpio.h"

#include "IMD.h"
#include "drv_inputAD.h"
#include "Module.h"
#include "Sys.h"
#include "SystemConfig.h"
#include "MessageUnpack_generated.h"
#include "NetworkDefines_generated.h"
#include "FeatureDefines_generated.h"

#define CURRENT_SENSE_V_per_A 0.005f

BMSB_S BMS;

void BMS_workerWatchdog(void);

static void BMS_init(void)
{
    memset(&BMS, 0x00, sizeof(BMSB_S));
    IMD_init();
}

static void BMS10Hz_PRD(void)
{
    BMSB_S tmp               = { 0x00 };
    tmp.fault                = false;
    tmp.pack_voltage         = 0.0f;
    tmp.voltages.max         = 0.0f;
    tmp.voltages.min         = 5.0f;
    tmp.max_temp             = 0.0f;
    tmp.pack_charge_limit    = BMS_MAX_CONT_CHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;
    tmp.pack_discharge_limit = 150.0f; //BMS_MAX_CONT_DISCHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;

    for (uint8_t i = 0; i < CAN_DUPLICATENODE_BMSW_COUNT; i++)
    {
        CAN_flag_E seg_faultFlag = 0U;
        const bool worker_valid = CANRX_get_signalDuplicate(VEH, BMSW_faultFlag, &seg_faultFlag, i) == CANRX_MESSAGE_VALID;
        if (worker_valid == false)
        {
            continue;
        }
        else if (worker_valid && (seg_faultFlag == CAN_FLAG_SET))
        {
#if BMS_FAULTS
            tmp.fault                = true;
            tmp.pack_discharge_limit = 0.0f;
            tmp.pack_charge_limit    = 0.0f;
#endif // BMS_FAULTS
        }

        float32_t charge_limit = 0.0f, discharge_limit = 0.0f, pack_voltage = 0.0f, max_voltage = 0.0f, min_voltage = 0.0f, max_temp = 0.0f;
        UNUSED(CANRX_get_signalDuplicate(VEH, BMSW_chargeLimit, &charge_limit, i));
        UNUSED(CANRX_get_signalDuplicate(VEH, BMSW_dischargeLimit, &discharge_limit, i));
        UNUSED(CANRX_get_signalDuplicate(VEH, BMSW_segmentVoltage, &pack_voltage, i));
        UNUSED(CANRX_get_signalDuplicate(VEH, BMSW_voltageMax, &max_voltage, i));
        UNUSED(CANRX_get_signalDuplicate(VEH, BMSW_voltageMin, &min_voltage, i));
        UNUSED(CANRX_get_signalDuplicate(VEH, BMSW_tempMax, &max_temp, i));
        tmp.pack_charge_limit    = (charge_limit < tmp.pack_charge_limit) ? charge_limit : tmp.pack_charge_limit;
        tmp.pack_discharge_limit = (discharge_limit < tmp.pack_discharge_limit) ? discharge_limit : tmp.pack_discharge_limit;

        tmp.pack_voltage += pack_voltage;

        tmp.voltages.max = (max_voltage > tmp.voltages.max) ? max_voltage : tmp.voltages.max;
        tmp.voltages.min = (min_voltage < tmp.voltages.min) ? min_voltage : tmp.voltages.min;
        tmp.max_temp     = (max_temp > tmp.max_temp) ? max_temp : tmp.max_temp;
    }

    BMS_workerWatchdog();

    if (BMS.connected_segments != BMS_CONFIGURED_SERIES_SEGMENTS)
    {
        tmp.fault                = true;
        BMS.pack_charge_limit    = 0U;
        BMS.pack_discharge_limit = 0U;
    }
    else
    {
        BMS.pack_charge_limit    = tmp.pack_charge_limit;
        BMS.pack_discharge_limit = tmp.pack_discharge_limit;
    }

    BMS.fault                = tmp.fault;
    BMS.pack_voltage         = tmp.pack_voltage;
    BMS.voltages             = tmp.voltages;
    BMS.max_temp             = tmp.max_temp;
}

static void BMS100Hz_PRD(void)
{
    BMS.pack_current = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_CS) / CURRENT_SENSE_V_per_A;

    if (BMS.fault || (SYS_SFT_checkMCTimeout() && SYS_SFT_checkElconChargerTimeout() && SYS_SFT_checkBrusaChargerTimeout()))
    {
        SYS_SFT_openShutdown();
    }
    else
    {
        SYS_SFT_closeShutdown();
    }

    if (IMD_getState() == IMD_HEALTHY)
    {
        HW_GPIO_writePin(HW_GPIO_IMD_STATUS, true);
    }
    else
    {
        HW_GPIO_writePin(HW_GPIO_IMD_STATUS, false);
    }

    if (BMS.fault ||
        (drv_inputAD_getLogicLevel(DRV_INPUTAD_DIGITAL_TSMS_CHG) == DRV_IO_LOGIC_LOW) ||
        (drv_inputAD_getLogicLevel(DRV_INPUTAD_DIGITAL_OK_HS) == DRV_IO_LOGIC_LOW))
    {
        SYS_SFT_openContactors();
    }
    else
    {
        if (drv_inputAD_getLogicLevel(DRV_INPUTAD_DIGITAL_TSMS_CHG) == DRV_IO_LOGIC_HIGH)
        {
            if (SYS.contacts == SYS_CONTACTORS_OPEN)
            {
                SYS_SFT_cycleContacts();
            }
            else if ((SYS.contacts == SYS_CONTACTORS_PRECHARGE) || (SYS.contacts == SYS_CONTACTORS_CLOSED))
            {
                float32_t ts_voltage = 0.0f;
                float32_t chg_voltage = 0.0f;
                const bool mc_valid = (CANRX_get_signal(VEH, PM100DX_tractiveSystemVoltage, &ts_voltage) == CANRX_MESSAGE_VALID);
                const bool chg_valid = (CANRX_get_signal(VEH, BRUSA513_dcBusVoltage, &chg_voltage) == CANRX_MESSAGE_VALID);
                if ((mc_valid || chg_valid) &&
                    ((ts_voltage > 0.95f * BMS.pack_voltage) || (chg_voltage > 0.95f * BMS.pack_voltage)))
                {
                    SYS_SFT_cycleContacts();
                }
            }
        }
    }
}

const ModuleDesc_S BMS_desc = {
    .moduleInit       = &BMS_init,
    .periodic10Hz_CLK = &BMS10Hz_PRD,
    .periodic100Hz_CLK = &BMS100Hz_PRD,
};

void BMS_workerWatchdog(void)
{
    BMS.connected_segments = 0;
    for (uint8_t i = 0; i < CAN_DUPLICATENODE_BMSW_COUNT; i++)
    {
        if (CANRX_validateDuplicate(VEH, BMSW_criticalData, i) == CANRX_MESSAGE_VALID)
        {
            BMS.connected_segments++;
#if BMS_FAULTS
            if (CANRX_get_signal_duplicateNode(VEH, BMSW, i, [i].max_temp > 60.0f || BMS.workers[i].voltages.max >= 4.2f ||
                CANRX_get_signal_duplicateNode(VEH, BMSW, i, [i].voltages.min <= 2.5f)
            {
                CANRX_get_signal_duplicateNode(VEH, BMSW, i, [i].fault = true;
            }
#endif // BMS_FAULTS
        }
    }
}
