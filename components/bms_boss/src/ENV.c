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
#include "drv_hih.h"
#include "IO.h"
#include "HW_i2c.h"


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
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static HW_I2C_Device_S i2c_hih = {
    .addr   =  0x27,
    .handle = &i2c,
};

static drv_hih_S hih_chip = {
    .dev = &i2c_hih,
};

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

    if (!drv_hih_init(&hih_chip))
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
    if (drv_hih_getState(&hih_chip) == DRV_HIH_MEASURING)
    {
        if (drv_hih_getData(&hih_chip))
        {
            ENV.board.ambient_temp = drv_hih_getTemperature(&hih_chip);
            ENV.board.rh           = drv_hih_getRH(&hih_chip);
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
            drv_hih_startConversion(&hih_chip);
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
