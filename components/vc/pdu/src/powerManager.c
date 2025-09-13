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

#define PDU_CS_AMPS_PER_VOLT 0.20f
#define PDU_VS_VOLTAGE_MULTIPLIER 3.61f

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
} powerManager_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t powerManager_getGLVVoltage(void)
{
    return powerManager_data.glv_voltage;
}

float32_t powerManager_getGLVCurrent(void)
{
    return powerManager_data.total_current;
}

static void powerManager_init(void)
{
    memset(&powerManager_data, 0x00, sizeof(powerManager_data));

    drv_tps2hb16ab_init();
    drv_vn9008_init();

    drv_tps2hb16ab_setDiagEnabled(DRV_TPS2HB16AB_IC_BMS1_SHUTDOWN, true); // All diag pins are set to the same gpio
    drv_tps2hb16ab_setCSChannel(DRV_TPS2HB16AB_IC_BMS1_SHUTDOWN, DRV_TPS2HB16AB_OUT_1);

    // Power of pump controlled by the cooling manager
    drv_vn9008_setCSEnabled(DRV_VN9008_CHANNEL_PUMP, true); // All sense enable pins are set to the same gpio
}

static void powerManager_periodic_10Hz(void)
{
    const float32_t glv_voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_UVL_BATT) * 6.62f;
    float32_t tmp_current = 0.0f;

    powerManager_data.glv_voltage = glv_voltage;

    const bool enable_loads = glv_voltage > 8.0f;
    const bool enable_shutdown = glv_voltage > 9.0f;

    const drv_io_activeState_E shutdown_en = enable_shutdown ? DRV_IO_ACTIVE : DRV_IO_INACTIVE;
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_VCU_SFTY_EN, shutdown_en);

    // TODO: Improve
    static drv_tps2hb16ab_output_E output = DRV_TPS2HB16AB_OUT_1;

    for (uint8_t i = 0; i < DRV_TPS2HB16AB_IC_COUNT; i++)
    {
        drv_tps2hb16ab_setEnabled(i, DRV_TPS2HB16AB_OUT_1, enable_loads);
        drv_tps2hb16ab_setEnabled(i, DRV_TPS2HB16AB_OUT_2, enable_loads);
        tmp_current += drv_tps2hb16ab_getCurrent(i, DRV_TPS2HB16AB_OUT_1);
        tmp_current += drv_tps2hb16ab_getCurrent(i, DRV_TPS2HB16AB_OUT_2);
    }
    output = (output + 1) % DRV_TPS2HB16AB_OUT_COUNT;
    drv_tps2hb16ab_setCSChannel(DRV_TPS2HB16AB_IC_BMS1_SHUTDOWN, output); // All CS select pins are on the same GPIO

    for (uint8_t i = 0; i < DRV_VN9008_CHANNEL_COUNT; i++)
    {
        tmp_current += drv_vn9008_getCurrent(i);
    }

    const float32_t pdu_current = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_DEMUX2_5V_SNS) * PDU_CS_AMPS_PER_VOLT;
    powerManager_data.pdu.current = pdu_current;
    powerManager_data.total_current = tmp_current + pdu_current;
    drv_tps2hb16ab_run();
    drv_vn9008_run();

    const float32_t pdu_5v_voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_5V_VOLTAGE) * PDU_VS_VOLTAGE_MULTIPLIER;
    powerManager_data.pdu.rail_5v_voltage = pdu_5v_voltage;
}

float32_t powerManager_getPduCurrent(void)
{
    return powerManager_data.pdu.current;
}

float32_t powerManager_getPdu5vVoltage(void)
{
    return powerManager_data.pdu.rail_5v_voltage;
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S powerManager_desc = {
    .moduleInit = &powerManager_init,
    .periodic10Hz_CLK = &powerManager_periodic_10Hz,
};
