/**
 * @file BMS.c
 * @brief  Source code for BMS manager
 */

#include "BMS.h"

#include <string.h>

#include "HW.h"

#include "IMD.h"
#include "drv_inputAD.h"
#include "drv_outputAD.h"
#include "drv_userInput.h"
#include "drv_timer.h"
#include "Module.h"
#include "SystemConfig.h"
#include "MessageUnpack_generated.h"
#include "NetworkDefines_generated.h"
#include "FeatureDefines_generated.h"
#include "lib_utility.h"

#define CURRENT_SENSE_V_per_A -0.0025f
#define PRECHARGE_MIN_TIME_MS 1320U

#define PACK_CS_0_OFFSET 0.0f

#define LOAD_CURRENT_THRESHOLD 5

#define CONTACTOR_SOH_LOW_WARN_THRESHOLD_PERCENTAGE 0.1f

BMSB_S BMS;

void BMS_SFT_openShutdown(void);
void BMS_SFT_closeShutdown(void);
void BMS_SFT_openContactors(void);
void BMS_SFT_cycleContacts(void);

static drv_timer_S precharge_timer;

float32_t BMSB_getContactorSohHvp(void)
{
    return SATURATE(
        0.0f,
        1.0f - ((float32_t)contactor_data.contactorLifetime.contactorHvp / BMSB_CONTACTOR_LIFETIME_HVC43),
        1.0f);
}

float32_t BMSB_getContactorSohHvn(void)
{
    return SATURATE(
        0.0f,
        1.0f - ((float32_t)contactor_data.contactorLifetime.contactorHvn / BMSB_CONTACTOR_LIFETIME_HVC43),
        1.0f);
}

float32_t BMSB_getContactorSohPrecharge(void)
{
    return SATURATE(
        0.0f,
        1.0f - ((float32_t)contactor_data.contactorLifetime.precharge / BMSB_CONTACTOR_LIFETIME_3350_PRECHARGE),
        1.0f);
}

uint32_t BMSB_getContactorLifetimeHvp(void)
{
    return contactor_data.contactorLifetime.contactorHvp;
}

uint32_t BMSB_getContactorLifetimeHvn(void)
{
    return contactor_data.contactorLifetime.contactorHvn;
}

uint32_t BMSB_getContactorLifetimePrecharge(void)
{
    return contactor_data.contactorLifetime.precharge;
}

static void BMS_init(void)
{
    memset(&BMS, 0x00, sizeof(BMSB_S));
    IMD_init();
    drv_timer_init(&precharge_timer);
    BMS.counted_coulombs.last_step_us = HW_TIM_getBaseTick();
    BMS.counted_coulombs.reset = true;

    lib_simpleFilter_lpf_calcSmoothingFactor(&BMS.lpfCurrent, 10.0f, 0.001f);
}

static void BMS10Hz_PRD(void)
{
    BMSB_S tmp                  = { 0x00 };
    tmp.fault                   = false;
    tmp.pack_voltage_calculated = 0.0f;
    tmp.pack_voltage_measured   = 0.0f;
    tmp.voltages.max            = 0.0f;
    tmp.voltages.min            = 5.0f;
    tmp.max_temp                = 0.0f;
    tmp.pack_charge_limit       = BMS_MAX_CONT_CHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;
    tmp.pack_discharge_limit    = 150.0f; //BMS_MAX_CONT_DISCHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;

    for (uint8_t i = 0; i < CAN_DUPLICATENODE_BMSW_COUNT; i++)
    {
        CAN_flag_E faultBMS = 0U;
        CAN_flag_E faultTemp = 0U;
        const bool worker_valid = CANRX_get_signalDuplicate(VEH, BMSW_faultBMS, &faultBMS, i) == CANRX_MESSAGE_VALID;
        (void)CANRX_get_signalDuplicate(VEH, BMSW_faultTemp, &faultTemp, i);
        if (worker_valid == false)
        {
            continue;
        }
        else if (worker_valid && ((faultBMS == CAN_FLAG_SET) || (faultTemp == CAN_FLAG_SET)))
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

        tmp.pack_voltage_calculated += pack_voltage;

        tmp.voltages.max = (max_voltage > tmp.voltages.max) ? max_voltage : tmp.voltages.max;
        tmp.voltages.min = (min_voltage < tmp.voltages.min) ? min_voltage : tmp.voltages.min;
        tmp.max_temp     = (max_temp > tmp.max_temp) ? max_temp : tmp.max_temp;
    }

    BMS.connected_segments = 0;
    for (uint8_t i = 0; i < CAN_DUPLICATENODE_BMSW_COUNT; i++)
    {
        if (CANRX_validateDuplicate(VEH, BMSW_criticalData, i) == CANRX_MESSAGE_VALID)
        {
            BMS.connected_segments++;
        }
    }

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

    BMS.pack_voltage_calculated = tmp.pack_voltage_calculated;
    BMS.fault        = tmp.fault;
    BMS.voltages     = tmp.voltages;
    BMS.max_temp     = tmp.max_temp;
}

static void BMS100Hz_PRD(void)
{
    const bool bmsFault = BMS.fault;
    const bool tsmsOpen = !drv_userInput_buttonPressed(USERINPUT_SWITCH_TSMS);
    const bool imdOpen = drv_inputAD_getLogicLevel(DRV_INPUTAD_DIGITAL_OK_HS) == DRV_IO_LOGIC_LOW;
    const bool timeout = BMS_SFT_checkMCTimeout() && BMS_SFT_checkElconChargerTimeout() && BMS_SFT_checkBrusaChargerTimeout();
    const bool openContactors = bmsFault || tsmsOpen || timeout;

    const bool underLoad = (BMS.pack_current > LOAD_CURRENT_THRESHOLD) || (BMS.pack_current < -LOAD_CURRENT_THRESHOLD);
    const bool contactorsClosed = (BMS.contacts == BMS_CONTACTORS_PRECHARGE) ||
                                  (BMS.contacts == BMS_CONTACTORS_CLOSED) ||
                                  (BMS.contacts == BMS_CONTACTORS_HVP_CLOSED);

    app_faultManager_setFaultState(FM_FAULT_BMSB_CONTACTORSOPENEDUNDERLOAD, openContactors && underLoad);
    app_faultManager_setFaultState(FM_FAULT_BMSB_BMSFAULTOPENEDCONTACTORS, bmsFault && contactorsClosed);
    app_faultManager_setFaultState(FM_FAULT_BMSB_TSMSOPENEDCONTACTORS, tsmsOpen && contactorsClosed);
    app_faultManager_setFaultState(FM_FAULT_BMSB_TIMEOUTOPENEDCONTACTORS, timeout && contactorsClosed);
    app_faultManager_setFaultState(FM_FAULT_BMSB_BMSFAULT, bmsFault);
    app_faultManager_setFaultState(FM_FAULT_BMSB_IMDNOK, imdOpen);
    app_faultManager_setFaultState(FM_FAULT_BMSB_TIMEOUT, timeout);
    app_faultManager_setFaultState(FM_FAULT_BMSB_CONTACTORLOWSOHHVP, BMSB_getContactorSohHvp() <  CONTACTOR_SOH_LOW_WARN_THRESHOLD_PERCENTAGE);
    app_faultManager_setFaultState(FM_FAULT_BMSB_CONTACTORLOWSOHHVN, BMSB_getContactorSohHvn() <  CONTACTOR_SOH_LOW_WARN_THRESHOLD_PERCENTAGE);
    app_faultManager_setFaultState(FM_FAULT_BMSB_CONTACTORLOWSOHPRECHARGE, BMSB_getContactorSohPrecharge() < CONTACTOR_SOH_LOW_WARN_THRESHOLD_PERCENTAGE);

    if (bmsFault)
    {
        BMS_SFT_openShutdown();
    }
    else
    {
        BMS_SFT_closeShutdown();
    }
#if APP_VARIANT_ID == 0U
    if (IMD_getState() == IMD_HEALTHY)
    {
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_STATUS_IMD, DRV_IO_ACTIVE);
    }
    else
    {
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_STATUS_IMD, DRV_IO_INACTIVE);
    }
#endif

    if (openContactors)
    {
        BMS_SFT_openContactors();
        drv_timer_stop(&precharge_timer);
    }
    else
    {
        if (drv_inputAD_getLogicLevel(DRV_INPUTAD_DIGITAL_TSMS_CHG) == DRV_IO_LOGIC_HIGH)
        {
            if (BMS.contacts == BMS_CONTACTORS_OPEN)
            {
                BMS_SFT_cycleContacts();
                drv_timer_start(&precharge_timer, PRECHARGE_MIN_TIME_MS);
                contactor_data.contactorLifetime.contactorHvn++;
                contactor_data.contactorLifetime.precharge++;
            }
            else if ((BMS.contacts == BMS_CONTACTORS_PRECHARGE) || (BMS.contacts == BMS_CONTACTORS_CLOSED))
            {
                float32_t ts_voltage = 0.0f;
                float32_t chg_voltage = 0.0f;
                const bool mc_valid = (CANRX_get_signal(VEH, PM100DX_tractiveSystemVoltage, &ts_voltage) == CANRX_MESSAGE_VALID);
                const bool chg_valid = (CANRX_get_signal(VEH, BRUSA513_dcBusVoltage, &chg_voltage) == CANRX_MESSAGE_VALID);

                if ((((mc_valid == true) && (ts_voltage > 0.95f * BMS_VPACK_SOURCE)) ||
                     ((chg_valid == true) && (chg_voltage > 0.95f * BMS_VPACK_SOURCE)))  &&
                    (drv_timer_getState(&precharge_timer) == DRV_TIMER_EXPIRED))
                {
                    BMS_SFT_cycleContacts();
                    contactor_data.contactorLifetime.contactorHvp++;
                    lib_nvm_requestWrite(NVM_ENTRYID_CONTACTOR_LIFETIME);
                }
            }
        }
    }
}

static void BMS1kHz_PRD(void)
{
    const uint64_t this_step = HW_TIM_getBaseTick();
    const uint32_t delta_t = (uint32_t)(this_step - BMS.counted_coulombs.last_step_us);

    float32_t tmpCurrent =  (drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_CS) - PACK_CS_0_OFFSET) / CURRENT_SENSE_V_per_A;
    BMS.packCurrentRaw = tmpCurrent;
    tmpCurrent = lib_simpleFilter_lpf_step(&BMS.lpfCurrent, tmpCurrent);
    BMS.pack_current = tmpCurrent;
    BMS.pack_voltage_measured = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_VPACK);
    BMS.pack_voltage_sense_fault = drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_VPACK_DIAG) == DRV_IO_ACTIVE;
    BMS.packPowerKW = (BMS.pack_voltage_measured * BMS.pack_current) / 1000.0f;

    // TODO: Update coulomb count in cells
    if ((BMS.contacts == BMS_CONTACTORS_PRECHARGE) ||
        (BMS.contacts == BMS_CONTACTORS_HVP_CLOSED) ||
        (BMS.contacts == BMS_CONTACTORS_CLOSED))
    {
        const float32_t delta_amp_hr = BMS.pack_current * (((float32_t)delta_t) / 1000000.0f) * (1.0f /  3600.0f);
        BMS.counted_coulombs.reset = false;
        BMS.counted_coulombs.amp_hr += delta_amp_hr;
        current_data.pack_amp_hours = SATURATE(0.0f,
                                               current_data.pack_amp_hours + delta_amp_hr,
                                               BMS_CONFIGURED_PARALLEL_CELLS * BMS_CELL_RATED_AMPHOURS * 1.5f);
    }
    else if (BMS.counted_coulombs.reset == false)
    {
        lib_nvm_requestWrite(NVM_ENTRYID_COULOMB_COUNT);
        BMS.counted_coulombs.amp_hr = 0.0f;
        BMS.counted_coulombs.reset = true;
    }

    BMS.counted_coulombs.last_step_us = this_step;
}

const ModuleDesc_S BMS_desc = {
    .moduleInit        = &BMS_init,
    .periodic10Hz_CLK  = &BMS10Hz_PRD,
    .periodic100Hz_CLK = &BMS100Hz_PRD,
    .periodic1kHz_CLK  = &BMS1kHz_PRD,
};

void BMS_SFT_openShutdown(void)
{
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_STATUS_BMS, DRV_IO_INACTIVE);
}

void BMS_SFT_closeShutdown(void)
{
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_STATUS_BMS, DRV_IO_ACTIVE);
}

void BMS_SFT_openContactors(void)
{
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_AIR, DRV_IO_INACTIVE);
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_PRECHG, DRV_IO_INACTIVE);
    BMS.contacts = BMS_CONTACTORS_OPEN;
}

void BMS_SFT_cycleContacts(void)
{
    if (BMS.contacts == BMS_CONTACTORS_OPEN)
    {
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_PRECHG, DRV_IO_ACTIVE);
        BMS.contacts = BMS_CONTACTORS_PRECHARGE;
    }
    else if (BMS.contacts == BMS_CONTACTORS_PRECHARGE)
    {
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_AIR, DRV_IO_ACTIVE);
        BMS.contacts = BMS_CONTACTORS_CLOSED;
    }
    else if (BMS.contacts == BMS_CONTACTORS_CLOSED)
    {
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_PRECHG, DRV_IO_INACTIVE);
        BMS.contacts = BMS_CONTACTORS_HVP_CLOSED;
    }
}

bool BMS_SFT_checkMCTimeout(void)
{
    return (CANRX_validate(VEH, PM100DX_criticalData) != CANRX_MESSAGE_VALID);
}

bool BMS_SFT_checkBrusaChargerTimeout(void)
{
    return (CANRX_validate(VEH, BRUSA513_criticalData) != CANRX_MESSAGE_VALID);
}

void BMS_stopCharging(void)
{
    BMS.charging_paused = true;
}

void BMS_continueCharging(void)
{
    BMS.charging_paused = false;
}

bool BMS_SFT_checkElconChargerTimeout(void)
{
    return (CANRX_validate(PRIVBMS, ELCON_criticalData) != CANRX_MESSAGE_VALID);
}
