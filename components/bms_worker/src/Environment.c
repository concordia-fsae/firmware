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
#include "IO.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern LTC2983_S ltc_chip;
extern HS4011_S  hs_chip;
extern IO_S IO;


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    MEASURING = 0x00,
    DONE
} Sensor_State_E;

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/**
 * @brief  Stores public struct for BMS Module
 */
Environment_S ENV;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static Sensor_State_E ltc_state;
static Sensor_State_E hs_state;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Environment Module Init function
 */
static void Environment_Init()
{
    ENV.state = ENV_INIT;

    if (!LTC_Init())
        ENV.state = ENV_ERROR;
    if (!HS4011_Init())
        ENV.state = ENV_ERROR;

    if (ENV.state != ENV_ERROR)
        ENV.state = ENV_RUNNING;
}

/**
 * @brief  10Hz Environment periodic function
 */
static void Environment10Hz_PRD()
{
    if (ltc_state == MEASURING)
    {
        if (LTC_GetMeasurement())
        {
            ltc_state = DONE;

            uint16_t tmp_min = UINT16_MAX;
            uint16_t tmp_max = 0x00;
            uint32_t tmp_avg = 0x00;

            for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
            {
                ENV.values.cells.cell_temps[i] = ltc_chip.temps[i];
                if (ltc_chip.temps[i] < tmp_min) tmp_min = ltc_chip.temps[i];
                if (ltc_chip.temps[i] > tmp_max) tmp_max = ltc_chip.temps[i];
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
        if (HS4011_GetData())
        {
            hs_state = DONE;

            ENV.values.board.ambient_temp = hs_chip.data.temp;
            ENV.values.board.rh = hs_chip.data.rh;
        }
    }

    ENV.values.board.mcu_temp = IO.temp.mcu * 10;
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
        case ENV_RUNNING:
            ltc_state = MEASURING;
            hs_state  = MEASURING;

            LTC_StartMeasurement();
            HS4011_StartConversion();
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
const ModuleDesc_S Environment_desc = {
    .moduleInit       = &Environment_Init,
    .periodic10Hz_CLK = &Environment10Hz_PRD,
    .periodic1Hz_CLK  = &Environment1Hz_PRD,
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/
