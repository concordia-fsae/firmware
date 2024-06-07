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

#include "PACK.h"

#ifndef BMS_WORKER_TIMEMOUT_MS
# define BMS_WORKER_TIMEOUT_MS 1000
#endif    // BMS_WORKER_TIMEOUT_MS

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
    tmp.pack_discharge_limit = BMS_MAX_CONT_DISCHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;

    for (uint8_t i = 0; i < BMS_MAX_SEGMENTS; i++)
    {
        if (BMS.workers[i].fault && !BMS.workers[i].timeout)
        {
            tmp.fault                = true;
            tmp.pack_discharge_limit = 0.0f;
            tmp.pack_charge_limit    = 0.0f;
        }
        else if (BMS.workers[i].timeout)
            continue;

        tmp.pack_charge_limit    = (BMS.workers[i].charge_limit < tmp.pack_charge_limit) ? BMS.workers[i].charge_limit : tmp.pack_charge_limit;
        tmp.pack_discharge_limit = (BMS.workers[i].discharge_limit < tmp.pack_discharge_limit) ? BMS.workers[i].discharge_limit : tmp.pack_discharge_limit;

        tmp.pack_voltage += BMS.workers[i].segment_voltage;

        tmp.voltages.max = (BMS.workers[i].voltages.max > tmp.voltages.max) ? BMS.workers[i].voltages.max : tmp.voltages.max;
        tmp.voltages.min = (BMS.workers[i].voltages.min < tmp.voltages.min) ? BMS.workers[i].voltages.min : tmp.voltages.min;
        tmp.max_temp     = (BMS.workers[i].max_temp > tmp.max_temp) ? BMS.workers[i].max_temp : tmp.max_temp;
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
                if ((SYS.ts.voltage > 0.95f * BMS.pack_voltage) ||
		    (SYS.charger.voltage > 0.95f * BMS.pack_voltage))
                {
                    SYS_SFT_cycleContacts();
                }
            }
            else if (SYS.contacts == SYS_CONTACTORS_CLOSED)
            {
                if ((SYS.ts.voltage > 0.97f * BMS.pack_voltage) ||
		    (SYS.charger.voltage > 0.95f * BMS.pack_voltage))
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
    for (uint8_t i = 0; i < BMS_MAX_SEGMENTS; i++)
    {
        BMS.workers[i].timeout = (BMS.workers[i].last_message + BMS_WORKER_TIMEOUT_MS < HW_getTick()) ? true : false;


        if (!BMS.workers[i].timeout)
            BMS.connected_segments++;
        //if (!BMS.workers[i].fault)
        //{
        //    if (BMS.workers[i].max_temp > 60.0f || BMS.workers[i].voltages.max >= 4.2f ||
        //        BMS.workers[i].voltages.min <= 2.5f)
        //    {
        //        BMS.workers[i].fault = true;
        //    }
        //}
    }
}

void BMS_setSegmentStats(uint8_t seg_id, const BMSW_S* seg)
{
    BMS.workers[seg_id & 0x07] = *seg;
}
