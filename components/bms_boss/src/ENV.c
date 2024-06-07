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
#include "ENV.h"

/**< System Includes */
#include <stdint.h>
#include <string.h>

/**< Driver Includes */
#include "HW_HIH.h"
#include "IO.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define TEMP_CHIP_V_PER_DEG_C 0.0043F    // [V/degC] slope of built-in temp sensor
#define TEMP_CHIP_V_AT_25_C   1.43F      // [V] voltage at 25 degC
#define TEMP_CHIP_FROM_V(v)   (((v - TEMP_CHIP_V_AT_25_C) / TEMP_CHIP_V_PER_DEG_C) + 25.0F)


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

ENV_S ENV;


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Environment Module Init function
 */
static void ENV_init()
{
    memset(&ENV, 0x00, sizeof(ENV));
    ENV.state = ENV_INIT;

    if (!HIH_init())
    {
        // ENV.state = ENV_ERROR;
    }

    if (ENV.state != ENV_ERROR)
        ENV.state = ENV_RUNNING;
}

/**
 * @brief  10Hz Environment periodic function
 */
static void ENV10Hz_PRD()
{
    if (hih_chip.data.state == HIH_MEASURING)
    {
        if (HIH_getData())
        {
            ENV.board.ambient_temp = ((float32_t)hih_chip.data.temp)/16382*165-40;
            ENV.board.rh           = ((float32_t)hih_chip.data.rh)/16382*100;
        }
    }

    ENV.board.mcu_temp = TEMP_CHIP_FROM_V(IO.mcu_temp);
}

/**
 * @brief  1Hz Environment periodic function
 */
static void ENV1Hz_PRD()
{
    switch (ENV.state)
    {
        case ENV_INIT:
            break;
        case ENV_RUNNING:
            HIH_startConversion();
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
    .moduleInit       = &ENV_init,
    .periodic10Hz_CLK = &ENV10Hz_PRD,
    .periodic1Hz_CLK  = &ENV1Hz_PRD,
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/
