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
    [DRV_INPUTAD_DIGITAL_SPARE1] = {
        .pin = HW_GPIO_DIG_SPARE1,
    },
    [DRV_INPUTAD_DIGITAL_SPARE2] = {
        .pin = HW_GPIO_DIG_SPARE2,
    },
    [DRV_INPUTAD_DIGITAL_SPARE3] = {
        .pin = HW_GPIO_DIG_SPARE3,
    },
    [DRV_INPUTAD_DIGITAL_SPARE4] = {
        .pin = HW_GPIO_DIG_SPARE4,
    },
    [DRV_INPUTAD_DIGITAL_5V_FLT1] = {
        .pin = HW_GPIO_5V_FLT1,
    },
    [DRV_INPUTAD_DIGITAL_5V_FLT2] = {
        .pin = HW_GPIO_5V_FLT2,
    },
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
    // This method only works since there is a 1:1 mapping from adc input to inputAD output
    for (uint8_t i = 0U; i < ADC_BANK1_CHANNEL_COUNT; i++)
    {
        drv_inputAD_private_setAnalogVoltage(i, HW_ADC_getVFromBank1Channel(i));
    }
    for (uint8_t i = 0U; i < ADC_BANK2_CHANNEL_COUNT; i++)
    {
        drv_inputAD_private_setAnalogVoltage(i + ADC_BANK1_CHANNEL_COUNT, HW_ADC_getVFromBank1Channel(i));
    }

    drv_inputAD_private_runDigital();
}

/**
 * @brief  IO Module descriptor
 */
const ModuleDesc_S IO_desc = {
    .moduleInit       = &drv_inputAD_init_componentSpecific,
    .periodic1kHz_CLK = &drv_inputAD_1kHz_PRD,
};
