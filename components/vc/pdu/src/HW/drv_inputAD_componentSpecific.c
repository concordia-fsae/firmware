/**
 * @file IO.c
 * @brief  Source code for IO Module
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module Header */
#include "drv_inputAD_private.h"

/**< System Includes*/
#include <string.h>

/**< Firmware Includes */
#include "HW.h"
#include "HW_adc.h"
#include "HW_dma.h"
#include "HW_gpio.h"

/**< Other Includes */
#include "ModuleDesc.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

drv_inputAD_configDigital_S drv_inputAD_configDigital[DRV_INPUTAD_DIGITAL_COUNT] = {
};

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  IO Module Init function
 */
static void drv_inputAD_init_componentSpecific(void)
{
    drv_inputAD_private_init();
    drv_inputAD_private_runDigital();
}

static void drv_inputAD_1kHz_PRD(void)
{
    // TODO: Populate inputAD analog voltages

    drv_inputAD_private_runDigital();
}

/**
 * @brief  IO Module descriptor
 */
const ModuleDesc_S IO_desc = {
    .moduleInit       = &drv_inputAD_init_componentSpecific,
    .periodic1kHz_CLK = &drv_inputAD_1kHz_PRD,
};
