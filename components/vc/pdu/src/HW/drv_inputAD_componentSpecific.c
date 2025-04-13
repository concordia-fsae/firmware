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
#include "MessageUnpack_generated.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

drv_inputAD_configDigital_S drv_inputAD_configDigital[DRV_INPUTAD_DIGITAL_COUNT] = {
    [DRV_INPUTAD_TSCHG_MS] = {
        .type = INPUT_DIGITAL_CAN,
        .config.canrx_digitalStatus = &CANRX_get_signal_func(VEH, BMSB_tsmsChg),
    },
    [DRV_INPUTAD_RUN_BUTTON] = {
        .type = INPUT_DIGITAL_CAN,
        .config.canrx_digitalStatus = &CANRX_get_signal_func(VEH, VCFRONT_runButtonStatus),
    },
};

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  IO Module Init function
 */
void drv_inputAD_init_componentSpecific(void)
{
    drv_inputAD_private_init();
    drv_inputAD_private_runDigital();
}

void drv_inputAD_1kHz_componentSpecific(void)
{
    // TODO: Populate inputAD analog voltages

    drv_inputAD_private_runDigital();
}
