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
 *                         P R I V A T E  V A R S
 ******************************************************************************/

drv_inputAD_configDigital_S drv_inputAD_configDigital[DRV_INPUTAD_DIGITAL_COUNT] = {
    [DRV_INPUTAD_DIGITAL_TSMS_CHG] = {
        .pin = HW_GPIO_TSMS_CHG,
    },
    [DRV_INPUTAD_DIGITAL_OK_HS] = {
        .pin = HW_GPIO_OK_HS,
    },
    [DRV_INPUTAD_DIGITAL_BMS_IMD_RESET] = {
        .pin = HW_GPIO_BMS_STATUS_MEM,
    },
    [DRV_INPUTAD_DIGITAL_IMD_STATUS_MEM] = {
        .pin = HW_GPIO_IMD_STATUS_MEM,
    },
    [DRV_INPUTAD_DIGITAL_BMS_STATUS_MEM] = {
        .pin = HW_GPIO_BMS_STATUS_MEM,
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
    const float32_t differential = HW_ADC_getVFromBank2Channel(ADC_BANK2_CHANNEL_CS_P) - HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_CS_N);

    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_CS, differential);
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_MCU_TEMP, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_MCU_TEMP));

    drv_inputAD_private_runDigital();
}

/**
 * @brief  IO Module descriptor
 */
const ModuleDesc_S drv_inputAD_desc = {
    .moduleInit       = &drv_inputAD_init_componentSpecific,
    .periodic1kHz_CLK = &drv_inputAD_1kHz_PRD,
};
