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
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_DIG_SPARE1,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_INPUTAD_DIGITAL_SPARE2] = {
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_DIG_SPARE2,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_INPUTAD_DIGITAL_SPARE3] = {
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_DIG_SPARE3,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_INPUTAD_DIGITAL_SPARE4] = {
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_DIG_SPARE4,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_INPUTAD_DIGITAL_5V_FLT1] = {
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_5V_FLT1,
            .active_level = DRV_IO_LOGIC_LOW, // TPS20xx has active low fault output
        },
    },
    [DRV_INPUTAD_DIGITAL_5V_FLT2] = {
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_5V_FLT2,
            .active_level = DRV_IO_LOGIC_LOW, // TPS20xx has active low fault output
        },
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
