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
#include "HW_SHT40.h"
#include "HW_NX3L4051PW.h"
#include "drv_inputAD.h"
#include "lib_thermistors.h"
#include "lib_voltageDivider.h"
#include "BatteryMonitoring.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define TEMP_CHIP_V_PER_DEG_C 0.0043F    // [V/degC] slope of built-in temp sensor
#define TEMP_CHIP_V_AT_25_C   1.43F      // [V] voltage at 25 degC
#define TEMP_CHIP_FROM_V(v)   (((v - TEMP_CHIP_V_AT_25_C) / TEMP_CHIP_V_PER_DEG_C) + 25.0F)

#define THERM_PULLUP  10000.0F
#define RES_FROM_V(v) (lib_voltageDivider_getRFromVKnownPullUp(v, THERM_PULLUP, ADC_REF_VOLTAGE))

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern SHT_S sht_chip;


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    SENS_INIT = 0x00,
    SENS_RUNNING,
    SENS_ERROR,
} Sensor_State_E;


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

ENV_S ENV;

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void ENV_calcTempStats(void);


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Environment Module Init function
 */
static void Environment_Init()
{
    ENV.state = ENV_INIT;

    if (ENV.state != ENV_ERROR)
    {
        ENV.state = ENV_RUNNING;
    }
}

/**
 * @brief  10Hz Environment periodic function
 */
static void Environment10Hz_PRD()
{
    if (sht_chip.state == SHT_MEASURING || sht_chip.state == SHT_HEATING)
    {
        if (SHT_getData())
        {
            ENV.values.board.ambient_temp = sht_chip.data.temp;
            ENV.values.board.rh           = sht_chip.data.rh;
        }
    }
    else if (sht_chip.state == SHT_WAITING)
    {
        SHT_startConversion();
    }

    ENV.values.board.mcu_temp       = TEMP_CHIP_FROM_V(drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MCU_TEMP));
    ENV.values.board.brd_temp[BRD1] = lib_thermistors_getCelsiusFromR_BParameter(&NCP21_bParam, RES_FROM_V(drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_BOARD_TEMP1)));
    ENV.values.board.brd_temp[BRD2] = lib_thermistors_getCelsiusFromR_BParameter(&NCP21_bParam, RES_FROM_V(drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_BOARD_TEMP2)));

    for (uint16_t i = 0; i < NX3L_MUX_COUNT; i++)
    {
        const float32_t mux1 = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX1_CH1 + i);
        const float32_t mux2 = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX2_CH1 + i);
        const float32_t mux3 = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX3_CH1 + i);
        ENV.values.temps[i].temp     = (mux1 > 0.25F && mux1 < 2.25F) ? 
                                        lib_thermistors_getCelsiusFromR_BParameter(&MF52_bParam, RES_FROM_V(mux1)) :
                                        0.0F;
        ENV.values.temps[i + 8].temp = (mux2 > 0.1F && mux2 < 2.9F) ?
                                        lib_thermistors_getCelsiusFromR_BParameter(&MF52_bParam, RES_FROM_V(mux2)) :
                                        0.0F;
        if (i < 4)
        {
            ENV.values.temps[i + 16].temp = (mux3 > 0.1F && mux3 < 2.9F) ? 
                                             lib_thermistors_getCelsiusFromR_BParameter(&MF52_bParam, RES_FROM_V(mux3)) :
                                             0.0F;
        }
    }

    ENV_calcTempStats();
}

/**
 * @brief  1Hz Environment periodic function
 */
static void Environment1Hz_PRD()
{
    switch (ENV.state)
    {
        case ENV_INIT:
            break;

        case ENV_FAULT:
        case ENV_RUNNING:
            if (sht_chip.state == SHT_WAITING)
            {
                if (ENV.values.board.rh > 90.0f)
                {
                    SHT_startHeater(SHT_HEAT_MED);
                }
                else
                {
                    SHT_startConversion();
                }
            }
            else if (sht_chip.state == SHT_ERROR)
            {
                // TODO: Implement error handling
            }
            break;

        case ENV_ERROR:
            break;

        default:
            break;
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


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Go through Environment variables and calculate segment statistics
 */
void ENV_calcTempStats(void)
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

    if (connected_channels < BMS_CONFIGURED_PARALLEL_CELLS * BMS_CONFIGURED_SERIES_CELLS * 0.2F)
    {
        ENV.state = ENV_ERROR;
    }
    else if (connected_channels < CHANNEL_COUNT)
    {
        ENV.state = ENV_FAULT;
    }

    ENV.values.avg_temp /= connected_channels;

    if (connected_channels <= (0.2f * BMS_CONFIGURED_PARALLEL_CELLS * BMS_CONFIGURED_SERIES_CELLS) ||
        connected_channels == 0 || ENV.values.max_temp >= 60)
    {
        ENV.state = ENV_FAULT;
    }
    else {
        ENV.state = ENV_RUNNING;
    }
}
