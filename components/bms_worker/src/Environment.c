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
#include "HW_HS4011.h"
#include "HW_SHT40.h"
#include "IO.h"
#include "THERMISTORS.h"
#include "NCP21XV103J03RA.h"
#include "MF52C1103F3380.h"
#include "BatteryMonitoring.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define TEMP_CHIP_V_PER_DEG_C 0.0043F    // [V/degC] slope of built-in temp sensor
#define TEMP_CHIP_V_AT_25_C   1.43F      // [V] voltage at 25 degC
#define TEMP_CHIP_FROM_V(v)   (((v - TEMP_CHIP_V_AT_25_C) / TEMP_CHIP_V_PER_DEG_C) + 25.0F)

#define THERM_PULLUP  10000
#define RES_FROM_V(v) (THERM_PULLUP * ((v / VREF) / (1 - (v / VREF))))


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

#if defined(BMSW_BOARD_VA1)
extern LTC2983_S ltc_chip;
extern HS4011_S  hs_chip;
#elif defined(BMSW_BOARD_VA3) /**< BMSW_BOARD_VA1 */
extern SHT_S sht_chip;
#endif                        /**< BMSW_BOARD_VA3 */
extern IO_S IO;


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
 *                         P R I V A T E  V A R S
 ******************************************************************************/

#if defined(BMSW_BOARD_VA1)
static Sensor_State_E ltc_state;
static Sensor_State_E hs_state;
#endif // BMSW_BOARD_VA3


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

#if defined(BMSW_BOARD_VA1)
    if (!LTC_init())
    {
        ENV.state = ENV_ERROR;
    }
    if (!HS4011_init())
    {
        ENV.state = ENV_ERROR;
    }
#elif defined(BMSW_BOARD_VA3) /**< BMSW_BOARD_VA1 */
    (void)SHT_init();
#endif                        /**< BMSW_BOARD_VA3 */

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
#if defined(BMSW_BOARD_VA1)
    if (ltc_state == MEASURING)
    {
        if (LTC_getMeasurement())
        {
            ltc_state = DONE;

            uint16_t tmp_min = UINT16_MAX;
            uint16_t tmp_max = 0x00;
            uint32_t tmp_avg = 0x00;

            for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
            {
                ENV.values.cells.cell_temps[i] = ltc_chip.temps[i];
                if (ltc_chip.temps[i] < tmp_min)
                {
                    tmp_min = ltc_chip.temps[i];
                }
                if (ltc_chip.temps[i] > tmp_max)
                {
                    tmp_max = ltc_chip.temps[i];
                }
                tmp_avg += ltc_chip.temps[i];
            }

            tmp_avg /= CHANNEL_COUNT;

            ENV.values.cells.avg_temp = (uint16_t)tmp_avg;
            ENV.values.cells.max_temp = tmp_max;
            ENV.values.cells.min_temp = tmp_min;
        }
    }

    if (hs_state == MEASURING)
    {
        if (HS4011_getData())
        {
            hs_state = DONE;

            ENV.values.board.ambient_temp = hs_chip.data.temp / 10;
            ENV.values.board.rh           = hs_chip.data.rh / 100;
        }
    }
#elif defined(BMSW_BOARD_VA3) /**< BMSW_BOARD_VA1 */
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

    ENV.values.board.mcu_temp       = TEMP_CHIP_FROM_V(IO.temp.mcu);
    ENV.values.board.brd_temp[BRD1] = (THERM_GetTempFromR_BParameter(&NCP21_bParam, RES_FROM_V(IO.temp.board[BRD1])) - KELVIN_OFFSET);
    ENV.values.board.brd_temp[BRD2] = (THERM_GetTempFromR_BParameter(&NCP21_bParam, RES_FROM_V(IO.temp.board[BRD2])) - KELVIN_OFFSET);

    for (uint16_t i = 0; i < NX3L_MUX_COUNT; i++)
    {
        ENV.values.temps[i].temp     = (IO.temp.mux1[i] > 0.25F && IO.temp.mux1[i] < 2.25F) ? (THERM_GetTempFromR_BParameter(&MF52_bParam, RES_FROM_V(IO.temp.mux1[i])) - KELVIN_OFFSET) : 0;
        ENV.values.temps[i + 8].temp = (IO.temp.mux2[i] > 0.1F && IO.temp.mux2[i] < 2.9F) ? (THERM_GetTempFromR_BParameter(&MF52_bParam, RES_FROM_V(IO.temp.mux2[i])) - KELVIN_OFFSET) : 0;
        if (i < 4)
        {
            ENV.values.temps[i + 16].temp = (IO.temp.mux3[i] > 0.1F && IO.temp.mux3[i] < 2.9F) ? (THERM_GetTempFromR_BParameter(&MF52_bParam, RES_FROM_V(IO.temp.mux3[i])) - KELVIN_OFFSET) : 0;
        }
    }

    ENV_calcTempStats();
#endif                        // if defined(BMSW_BOARD_VA1)
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
#if defined(BMSW_BOARD_VA1)
            ltc_state = MEASURING;
            hs_state  = MEASURING;

            LTC_startMeasurement();
            HS4011_startConversion();
#elif defined(BMSW_BOARD_VA3) /**< BMSW_BOARD_VA1 */
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
#endif
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
