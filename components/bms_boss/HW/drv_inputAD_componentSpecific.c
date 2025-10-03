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
#include "LIB_simpleFilter.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define VPACK_DIVISOR 250

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

drv_inputAD_configDigital_S drv_inputAD_configDigital[DRV_INPUTAD_DIGITAL_COUNT] = {
    [DRV_INPUTAD_DIGITAL_TSMS_CHG] = {
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_TSMS_CHG,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_INPUTAD_DIGITAL_OK_HS] = {
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_OK_HS,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_INPUTAD_DIGITAL_BMS_IMD_RESET] = {
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_BMS_IMD_RESET,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_INPUTAD_DIGITAL_IMD_STATUS_MEM] = {
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_IMD_STATUS_MEM,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_INPUTAD_DIGITAL_BMS_STATUS_MEM] = {
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_BMS_STATUS_MEM,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_INPUTAD_DIGITAL_VPACK_DIAG] = {
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_VPACK_DIAG,
            .active_level = DRV_IO_LOGIC_LOW,
        },
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
    HW_ADC_unpackADCBuffer();

    const float32_t vcs_diff = HW_ADC_getVFromBank1Channel(ADC_BANK_CHANNEL_CS) - HW_ADC_getVFromBank2Channel(ADC_BANK_CHANNEL_CS);

    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_CS, vcs_diff);
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_MCU_TEMP, HW_ADC_getVFromBank1Channel(ADC_BANK_CHANNEL_MCU_TEMP));
#if BMSB_CONFIG_ID == 1U
    const float32_t vpack_p = HW_ADC_getVFromBank1Channel(ADC_BANK_CHANNEL_VPACK);
    const float32_t vpack_n = HW_ADC_getVFromBank2Channel(ADC_BANK_CHANNEL_VPACK);
    const float32_t vpack_diff = vpack_p - vpack_n;
    drv_inputAD_private_setRawAnalogVoltage(DRV_INPUTAD_ANALOG_VPACK, vpack_diff * VPACK_DIVISOR);
#endif

    drv_inputAD_private_runDigital();
}

/**
 * @brief  IO Module descriptor
 */
const ModuleDesc_S drv_inputAD_desc = {
    .moduleInit       = &drv_inputAD_init_componentSpecific,
    .periodic1kHz_CLK = &drv_inputAD_1kHz_PRD,
};
