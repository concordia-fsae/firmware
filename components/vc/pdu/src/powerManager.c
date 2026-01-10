/**
 * @file powerManager.c
 * @brief Module source that manages the power outputs of the board
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "powerManager.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "drv_tps2hb16ab.h"
#include "drv_vn9008.h"
#include "LIB_Types.h"
#include "string.h"
#include "drv_inputAD.h"
#include "drv_outputAD.h"
#include "app_faultManager.h"
#include "app_vehicleState.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define PDU_CS_AMPS_PER_VOLT 0.20f
#define PDU_VS_VOLTAGE_MULTIPLIER 3.61f

// System design minimums
// MINIMUM_TS_ACTIVE 9.0V
// MINIMUM_BOARD     8.0V
#define BATTERY_OVERVOLTAGE    15.0f
#define BATTERY_RECHARGED      12.25f
#define BATTERY_CUTOFF_SFTY_HI 11.75f
#define BATTERY_CUTOFF_SFTY_LO 11.50f
#define BATTERY_CUTOFF_LOAD_HI 11.25f
#define BATTERY_CUTOFF_LOAD_LO 11.00f
#define BATTERY_CUTOFF_ANY_HI  10.75f
#define BATTERY_CUTOFF_ANY_LO  10.50f

// Various power domain sanity checks
_Static_assert(BATTERY_CUTOFF_ANY_LO < BATTERY_CUTOFF_ANY_HI, "Battery cutoff low must be lower than the high threshold.");
_Static_assert(BATTERY_CUTOFF_LOAD_LO < BATTERY_CUTOFF_LOAD_HI, "Load cutoff low must be lower than the high threshold.");
_Static_assert(BATTERY_CUTOFF_SFTY_LO < BATTERY_CUTOFF_SFTY_HI, "Safety cutoff low must be lower than the high threshold.");

_Static_assert(BATTERY_CUTOFF_ANY_HI < BATTERY_CUTOFF_LOAD_LO, "Battery cutoff must be lower than the load cutoff.");
_Static_assert(BATTERY_CUTOFF_LOAD_HI < BATTERY_CUTOFF_SFTY_LO, "Load cutoff must be lower than the safety cutoff.");
_Static_assert(BATTERY_CUTOFF_SFTY_HI <= BATTERY_RECHARGED, "Battery must be considered recharged at a higher voltage than the highest cutoff.");
_Static_assert(BATTERY_RECHARGED < BATTERY_OVERVOLTAGE, "Overvoltage must be highest voltage value.");

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    float32_t total_current;
    float32_t glv_voltage;
    struct {
        float32_t current;
        float32_t rail_5v_voltage;
    } pdu;

    bool okBattery:1;
    bool okLoads:1;
    bool okSafety:1;

    bool charged:1;
    bool sleeping:1;
    bool lowBattery:1;
    bool overvoltage:1;
} pm_data;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void getInputs(void)
{
    CAN_prechargeContactorState_E contactor_state = CAN_PRECHARGECONTACTORSTATE_SNA;
    const bool contactorsValid = CANRX_get_signal(VEH, BMSB_packContactorState, &contactor_state) == CANRX_MESSAGE_VALID;
    const bool contactorsOpen = contactor_state != CAN_PRECHARGECONTACTORSTATE_HVP_CLOSED;
    const bool inRunMode = app_vehicleState_getState() == VEHICLESTATE_TS_RUN;

    app_faultManager_setFaultState(FM_FAULT_VCPDU_CONTACTSOPENINRUN, inRunMode && (!contactorsValid || contactorsOpen));

    pm_data.glv_voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_UVL_BATT) * 6.62f;
    pm_data.pdu.current = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_DEMUX2_5V_SNS) * PDU_CS_AMPS_PER_VOLT;
    pm_data.pdu.rail_5v_voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_5V_VOLTAGE) * PDU_VS_VOLTAGE_MULTIPLIER;
    pm_data.sleeping = app_vehicleState_sleeping();
}

static void evalAbilities(void)
{
    const bool resetFaults = app_vehicleState_getFaultReset();
    const bool lowBattery = pm_data.glv_voltage < BATTERY_RECHARGED;
    const bool charged = pm_data.charged ?
                         pm_data.glv_voltage > BATTERY_CUTOFF_ANY_LO :
                         pm_data.glv_voltage > BATTERY_RECHARGED;
    const bool okBattery = pm_data.okBattery ?
                           pm_data.glv_voltage > BATTERY_CUTOFF_ANY_LO :
                           pm_data.glv_voltage > BATTERY_CUTOFF_ANY_HI;
    const bool okLoads = pm_data.okLoads ?
                         pm_data.glv_voltage > BATTERY_CUTOFF_LOAD_LO :
                         pm_data.glv_voltage > BATTERY_CUTOFF_LOAD_HI;
    const bool okSafety = pm_data.okSafety ?
                          pm_data.glv_voltage > BATTERY_CUTOFF_SFTY_LO :
                          pm_data.glv_voltage > BATTERY_CUTOFF_SFTY_HI && resetFaults;
    const bool overvoltage = pm_data.glv_voltage > BATTERY_OVERVOLTAGE;

    pm_data.lowBattery = lowBattery;
    pm_data.charged = charged;
    pm_data.okBattery = okBattery && !overvoltage;
    pm_data.okLoads = okLoads && !pm_data.sleeping && !overvoltage && charged;
    pm_data.okSafety = okSafety && !pm_data.sleeping && !overvoltage && charged;

    app_faultManager_setFaultState(FM_FAULT_VCPDU_LOWVOLTAGE, lowBattery);
    app_faultManager_setFaultState(FM_FAULT_VCPDU_OVERVOLTAGE, overvoltage);
    app_faultManager_setFaultState(FM_FAULT_VCPDU_BATTERYNOK, !okBattery);
    app_faultManager_setFaultState(FM_FAULT_VCPDU_LOADSNOK, !okLoads);
    app_faultManager_setFaultState(FM_FAULT_VCPDU_SAFETYNOK, !okSafety);
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t powerManager_getGLVVoltage(void)
{
    return pm_data.glv_voltage;
}

float32_t powerManager_getGLVCurrent(void)
{
    return pm_data.total_current;
}

static void powerManager_init(void)
{
    memset(&pm_data, 0x00, sizeof(pm_data));

    drv_tps2hb16ab_init();
    drv_vn9008_init();

    drv_tps2hb16ab_setDiagEnabled(DRV_TPS2HB16AB_IC_BMS1_SHUTDOWN, true); // All diag pins are set to the same gpio
    drv_tps2hb16ab_setCSChannel(DRV_TPS2HB16AB_IC_BMS1_SHUTDOWN, DRV_TPS2HB16AB_OUT_1);

    // Power of pump controlled by the cooling manager
    drv_vn9008_setCSEnabled(DRV_VN9008_CHANNEL_PUMP, true); // All sense enable pins are set to the same gpio

    drv_tps2hb16ab_setFaultLatch(DRV_TPS2HB16AB_IC_VCU1_VCU2, true);
    drv_tps2hb16ab_setEnabled(DRV_TPS2HB16AB_IC_VCU1_VCU2, DRV_TPS2HB16AB_OUT_1, true);
    drv_tps2hb16ab_setEnabled(DRV_TPS2HB16AB_IC_VCU1_VCU2, DRV_TPS2HB16AB_OUT_2, true);
    drv_tps2hb16ab_run();
}

static void powerManager_periodic_100Hz(void)
{
    float32_t tmp_current = 0.0f;

    getInputs();
    evalAbilities();

    const drv_io_activeState_E shutdown_en = pm_data.okSafety ? DRV_IO_ACTIVE : DRV_IO_INACTIVE;
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_VCU_SFTY_EN, shutdown_en);

    // TODO: Improve
    static drv_tps2hb16ab_output_E output = DRV_TPS2HB16AB_OUT_1;

    for (uint8_t i = 0; i < DRV_TPS2HB16AB_IC_COUNT; i++)
    {
        for (uint8_t n = 0; n < DRV_TPS2HB16AB_OUT_COUNT; n++)
        {
            // Handle specific HSDs by sleep state
            const bool requiredLoad = (((i == DRV_TPS2HB16AB_IC_VC1_VC2) && (n == DRV_TPS2HB16AB_OUT_1)) ||
                                       (i == DRV_TPS2HB16AB_IC_VCU1_VCU2));
            const bool enableLoad = pm_data.okLoads || (requiredLoad && pm_data.okBattery);

            drv_hsd_state_E state = drv_tps2hb16ab_getState(i, n);
            if (state == DRV_HSD_STATE_OVERCURRENT)
            {
                // TODO: Set alarm
                drv_tps2hb16ab_setEnabled(i, n, false);
            }
            else if ((state == DRV_HSD_STATE_OFF) || (state == DRV_HSD_STATE_ON))
            {
                drv_tps2hb16ab_setEnabled(i, n, enableLoad);
            }
        }
    }
    drv_tps2hb16ab_setFaultLatch(DRV_TPS2HB16AB_IC_MC_VCU3, app_vehicleState_getState() != VEHICLESTATE_INIT);
    drv_tps2hb16ab_setFaultLatch(DRV_TPS2HB16AB_IC_VCU1_VCU2, app_vehicleState_getState() != VEHICLESTATE_INIT);

    drv_tps2hb16ab_run();
    drv_vn9008_run();

    for (uint8_t i = 0; i < DRV_TPS2HB16AB_IC_COUNT; i++)
    {
        tmp_current += drv_tps2hb16ab_getCurrent(i, DRV_TPS2HB16AB_OUT_1);
        tmp_current += drv_tps2hb16ab_getCurrent(i, DRV_TPS2HB16AB_OUT_2);
    }
    for (uint8_t i = 0; i < DRV_VN9008_CHANNEL_COUNT; i++)
    {
        tmp_current += drv_vn9008_getCurrent(i);
    }

    pm_data.total_current = tmp_current + pm_data.pdu.current;

    output = (output + 1) % DRV_TPS2HB16AB_OUT_COUNT;
    drv_tps2hb16ab_setCSChannel(DRV_TPS2HB16AB_IC_BMS1_SHUTDOWN, output); // All CS select pins are on the same GPIO
}

float32_t powerManager_getPduCurrent(void)
{
    return pm_data.pdu.current;
}

float32_t powerManager_getPdu5vVoltage(void)
{
    return pm_data.pdu.rail_5v_voltage;
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S powerManager_desc = {
    .moduleInit = &powerManager_init,
    .periodic100Hz_CLK = &powerManager_periodic_100Hz,
};
