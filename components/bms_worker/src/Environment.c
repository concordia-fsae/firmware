/**
 * @file Environment.h
 * @brief  Header file for Environment sensors
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-27
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module header */
#include "Environment.h"

/**< System Includes */
#include <stdint.h>

/**< Driver Includes */
#include "HW_adc.h"
#include "drv_sht40.h"
#include "drv_inputAD.h"
#include "drv_tempSensors.h"
#include "drv_timer.h"
#include "lib_voltageDivider.h"
#include "BatteryMonitoring.h"
#include "Module.h"
#include "app_faultManager.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define TEMP_CHIP_V_PER_DEG_C 0.0043F    // [V/degC] slope of built-in temp sensor
#define TEMP_CHIP_V_AT_25_C   1.43F      // [V] voltage at 25 degC
#define TEMP_CHIP_FROM_V(v)   (((v - TEMP_CHIP_V_AT_25_C) / TEMP_CHIP_V_PER_DEG_C) + 25.0F)

#define THERM_PULLUP  10000.0F
#define RES_FROM_V(v) (lib_voltageDivider_getRFromVKnownPullUp(v, THERM_PULLUP, ADC_REF_VOLTAGE))

#define ENV_ERROR_TIMEOUT_MS 1000U

#if APP_VARIANT_ID == 0U
#define CELL_THERM_BPARAM MF52_bParam
#elif APP_VARIANT_ID == 1U
#define CELL_THERM_BPARAM NTC103JT_bParam
#else
#error "Variant not supported"
#endif

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern HW_I2C_Handle_T i2c2;

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

ENV_S ENV;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

HW_I2C_Device_S I2C_SHT40 = {
    .addr   =  0x44,
    .handle = &i2c2,
};

drv_sht40_S sht_chip = {
    .dev = &I2C_SHT40,
};

static struct env_S
{
    drv_timer_S timerDisconnected;
    drv_timer_S timerInsufficient;
} env;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Go through Environment variables and calculate segment statistics
 */
static void ENV_calcTempStats(void)
{
    uint8_t connected_channels = 0;

    ENV.values.avg_temp = 0;
    ENV.values.max_temp = 0;
    ENV.values.min_temp = 200;

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        if ((uint8_t)ENV.values.temps[i].temp == 0)
        {
            ENV.values.temps[i].therm_error = true;
            continue;
        }

        connected_channels++;
        ENV.values.temps[i].therm_error = false;
        ENV.values.avg_temp += ENV.values.temps[i].temp;
        ENV.values.max_temp = (ENV.values.temps[i].temp > ENV.values.max_temp) ? ENV.values.temps[i].temp : ENV.values.max_temp;
        ENV.values.min_temp = (ENV.values.temps[i].temp < ENV.values.min_temp) ? ENV.values.temps[i].temp : ENV.values.min_temp;
    }

    ENV.values.avg_temp /= connected_channels;

    const bool disconnectedCell = connected_channels != CHANNEL_COUNT;
    const bool insufficientThermistors = connected_channels <= (0.2f * BMS_CONFIGURED_PARALLEL_CELLS * BMS_CONFIGURED_SERIES_CELLS);
    const bool thermistorFaulted = drv_timer_run(&env.timerDisconnected, disconnectedCell) == DRV_TIMER_EXPIRED;
    const bool insufficientFaulted = drv_timer_run(&env.timerInsufficient, insufficientThermistors) == DRV_TIMER_EXPIRED;
    const bool cellOvertemp = ENV.values.max_temp >= 60;

    app_faultManager_setFaultState(FM_FAULT_BMSW_THERMISTORDISCONNECTED, disconnectedCell);
    app_faultManager_setFaultState(FM_FAULT_BMSW_INSUFFICIENTTHERMISTORS, insufficientThermistors);
    app_faultManager_setFaultState(FM_FAULT_BMSW_CELLOVERTEMP, cellOvertemp);

    ENV.fault = (insufficientFaulted || thermistorFaulted || cellOvertemp);
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Environment Module Init function
 */
static void Environment_Init()
{
    ENV.fault = false;
    drv_timer_initWithRuntime(&env.timerDisconnected, ENV_ERROR_TIMEOUT_MS);
    drv_timer_initWithRuntime(&env.timerInsufficient, ENV_ERROR_TIMEOUT_MS);

    drv_sht40_init(&sht_chip);
}

/**
 * @brief  10Hz Environment periodic function
 */
static void Environment10Hz_PRD()
{
    if (sht_chip.state == SHT40_MEASURING || sht_chip.state == SHT40_HEATING)
    {
        if (drv_sht40_getData(&sht_chip))
        {
            ENV.values.board.ambient_temp = sht_chip.data.temp;
            ENV.values.board.rh           = sht_chip.data.rh;
        }
    }
    else if (sht_chip.state == SHT40_WAITING)
    {
        if (ENV.startRhHeater && (ENV.values.board.rh > 90.0f))
        {
            ENV.startRhHeater = false;
            drv_sht40_startHeater(&sht_chip, SHT40_HEAT_MED);
        }
        else
        {
            drv_sht40_startConversion(&sht_chip);
        }
    }

    for (uint16_t i = 0; i <= DRV_INPUTAD_ANALOG_MUX1_CH8 - DRV_INPUTAD_ANALOG_MUX1_CH1; i++)
    {
        const float32_t mux1 = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX1_CH1 + i);
#if APP_VARIANT_ID == 0U
        const float32_t mux2 = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX2_CH1 + i);
        const float32_t mux3 = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX3_CH1 + i);
#endif
        ENV.values.temps[i].temp     = (mux1 > 0.25F && mux1 < 2.25F) ? 
                                        lib_thermistors_getCelsiusFromR_BParameter(&CELL_THERM_BPARAM, RES_FROM_V(mux1)) :
                                        0.0F;
#if APP_VARIANT_ID == 0U
        ENV.values.temps[i + 8].temp = (mux2 > 0.1F && mux2 < 2.9F) ?
                                        lib_thermistors_getCelsiusFromR_BParameter(&CELL_THERM_BPARAM, RES_FROM_V(mux2)) :
                                        0.0F;
        if (i < 4)
        {
            ENV.values.temps[i + 16].temp = (mux3 > 0.1F && mux3 < 2.9F) ? 
                                             lib_thermistors_getCelsiusFromR_BParameter(&CELL_THERM_BPARAM, RES_FROM_V(mux3)) :
                                             0.0F;
        }
#endif
    }
#if APP_VARIANT_ID == 1U
    const float32_t therm9 = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_TEMP_THERM9);
    ENV.values.temps[CH9].temp     = (therm9 > 0.25F && therm9 < 2.25F) ? 
                                      lib_thermistors_getCelsiusFromR_BParameter(&CELL_THERM_BPARAM, RES_FROM_V(therm9)) :
                                      0.0F;
#endif

    ENV_calcTempStats();
}

/**
 * @brief  1Hz Environment periodic function
 */
static void Environment1Hz_PRD()
{
    if (sht_chip.state == SHT40_ERROR)
    {
        // TODO: Implement error handling
    }
    else
    {
        ENV.startRhHeater = true;
    }
}

/**
 * @brief  Environment Module decriptor
 */
const ModuleDesc_S ENV_desc = {
    .moduleInit       = &Environment_Init,
    .periodic10Hz_CLK = &Environment10Hz_PRD,
    .periodic1Hz_CLK  = &Environment1Hz_PRD,
};
