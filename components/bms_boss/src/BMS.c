/**
 * @file BMS.c
 * @brief  Source code for BMS manager
 */

#include "BMS.h"

#include <string.h>

#include "HW.h"
#include "HW_gpio.h"

#include "IMD.h"
#include "IO.h"
#include "Module.h"
#include "Sys.h"
#include "SystemConfig.h"
#include "MessageUnpack_generated.h"

#include "PACK.h"

BMSB_S BMS;

HW_GPIO_S imd_status = {
    .pin  = IMD_STATUS_Pin,
    .port = IMD_STATUS_Port,
};

HW_GPIO_S bms_status = {
    .pin  = BMS_STATUS_Pin,
    .port = BMS_STATUS_Port,
};

void BMS_workerWatchdog(void);

static void BMS_init(void)
{
    memset(&BMS, 0x00, sizeof(BMSB_S));
    IMD_init();
}

static void BMS10Hz_PRD(void)
{
    BMS_workerWatchdog();

    BMSB_S tmp               = { 0x00 };
    tmp.fault                = false;
    tmp.pack_voltage         = 0.0f;
    tmp.voltages.max         = 0.0f;
    tmp.voltages.min         = 5.0f;
    tmp.max_temp             = 0.0f;
    tmp.pack_charge_limit    = BMS_MAX_CONT_CHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;
    tmp.pack_discharge_limit = 150.0f; //BMS_MAX_CONT_DISCHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;

    for (uint8_t i = 0; i < BMS_CONFIGURED_SEGMENTS; i++)
    {
        if ((CANRX_get_signal_duplicateNode(VEH, BMSW, 0, faultFlag).value == true) &&
            (CANRX_get_signal_duplicateNode(VEH, BMSW, 0, faultFlag).health == CANRX_MESSAGE_VALID))
        {
            tmp.fault                = true;
            tmp.pack_discharge_limit = 0.0f;
            tmp.pack_charge_limit    = 0.0f;
        }
        else if (CANRX_get_signal_duplicateNode(VEH, BMSW, 0, faultFlag).health == CANRX_MESSAGE_SNA)
            continue;

        tmp.pack_charge_limit    = (CANRX_get_signal_duplicateNode(VEH, BMSW, 0, chargeLimit).value < tmp.pack_charge_limit) ? CANRX_get_signal_duplicateNode(VEH, BMSW, 0, chargeLimit).value : tmp.pack_charge_limit;
        tmp.pack_discharge_limit = (CANRX_get_signal_duplicateNode(VEH, BMSW, 0, dischargeLimit).value < tmp.pack_discharge_limit) ? CANRX_get_signal_duplicateNode(VEH, BMSW, 0, dischargeLimit).value : tmp.pack_discharge_limit;

        tmp.pack_voltage += CANRX_get_signal_duplicateNode(VEH, BMSW, 0, segmentVoltage).value;

        tmp.voltages.max = (CANRX_get_signal_duplicateNode(VEH, BMSW, 0, voltageMax).value > tmp.voltages.max) ? CANRX_get_signal_duplicateNode(VEH, BMSW, 0, voltageMax).value : tmp.voltages.max;
        tmp.voltages.min = (CANRX_get_signal_duplicateNode(VEH, BMSW, 0, voltageMin).value < tmp.voltages.min) ? CANRX_get_signal_duplicateNode(VEH, BMSW, 0, voltageMin).value : tmp.voltages.min;
        tmp.max_temp     = (CANRX_get_signal_duplicateNode(VEH, BMSW, 0, tempMax).value > tmp.max_temp) ? CANRX_get_signal_duplicateNode(VEH, BMSW, 0, tempMax).value : tmp.max_temp;
    }

    if (BMS.connected_segments != BMS_CONFIGURED_SEGMENTS)
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
    if (BMS.fault || (SYS_SFT_checkMCTimeout() && SYS_SFT_checkChargerTimeout()))
    {
        SYS_SFT_openShutdown();
    }
    else
    {
        SYS_SFT_closeShutdown();
    }

    if (IMD_getState() == IMD_HEALTHY)
    {
        HW_GPIO_writePin(&imd_status, true);
    }
    else
    {
        HW_GPIO_writePin(&imd_status, false);
    }

    if (BMS.fault || !IO.imd_ok || !IO.master_switch)
    {
        SYS_SFT_openContactors();
    }
    else
    {
        if (IO.master_switch)
        {
            if (SYS.contacts == SYS_CONTACTORS_OPEN)
            {
                SYS_SFT_cycleContacts();
            }
            else if (SYS.contacts == SYS_CONTACTORS_PRECHARGE)
            {
                if (((CANRX_get_signal(VEH, PM100DX_tractiveSystemVoltage).value > 0.95f * BMS.pack_voltage) && 
                     (CANRX_get_signal(VEH, PM100DX_tractiveSystemVoltage).health == CANRX_MESSAGE_VALID)) ||
		            ((CANRX_get_signal(VEH, BRUSA513_chargerBusVoltage).value > 0.95f * BMS.pack_voltage) &&
                     (CANRX_get_signal(VEH, BRUSA513_chargerBusVoltage).health == CANRX_MESSAGE_VALID)))
                {
                    SYS_SFT_cycleContacts();
                }
            }
            else if (SYS.contacts == SYS_CONTACTORS_CLOSED)
            {
                if (((CANRX_get_signal(VEH, PM100DX_tractiveSystemVoltage).value > 0.97f * BMS.pack_voltage) && 
                     (CANRX_get_signal(VEH, PM100DX_tractiveSystemVoltage).health == CANRX_MESSAGE_VALID)) ||
		            ((CANRX_get_signal(VEH, BRUSA513_chargerBusVoltage).value > 0.97f * BMS.pack_voltage) &&
                     (CANRX_get_signal(VEH, BRUSA513_chargerBusVoltage).health == CANRX_MESSAGE_VALID)))
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
    for (uint8_t i = 0; i < BMS_CONFIGURED_SEGMENTS; i++)
    {
        if (CANRX_get_signal_duplicateNode(VEH, BMSW, 0, voltageMin).health == CANRX_MESSAGE_VALID)
            BMS.connected_segments++;
        //if (!CANRX_get_signal_duplicateNode(VEH, BMSW, i, [i].fault)
        //{
        //    if (CANRX_get_signal_duplicateNode(VEH, BMSW, i, [i].max_temp > 60.0f || BMS.workers[i].voltages.max >= 4.2f ||
        //        CANRX_get_signal_duplicateNode(VEH, BMSW, i, [i].voltages.min <= 2.5f)
        //    {
        //        CANRX_get_signal_duplicateNode(VEH, BMSW, i, [i].fault = true;
        //    }
        //}
    }
}
