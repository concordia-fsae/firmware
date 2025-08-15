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
#include "drv_mux.h"

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
        .config.canrx_digitalStatus = &CANRX_get_signal_func(VEH, STW_runButton),
    },
    [DRV_INPUTAD_IMD_SAFETY_EN] = {
        .type = INPUT_DIGITAL_CAN,
        .config.canrx_digitalStatus = &CANRX_get_signal_func(VEH, BMSB_imdStatusMem),
    },
    [DRV_INPUTAD_BMS_SAFETY_EN] = {
        .type = INPUT_DIGITAL_CAN,
        .config.canrx_digitalStatus = &CANRX_get_signal_func(VEH, BMSB_bmsStatusMem),
    },
    [DRV_INPUTAD_BMS_RESET] = {
        .type = INPUT_DIGITAL_CAN,
        .config.canrx_digitalStatus = &CANRX_get_signal_func(VEH, BMSB_bmsIMDReset),
    },
    [DRV_INPUTAD_5V_NFLT1] = { 
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_5V_NFLT1,
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_INPUTAD_5V_NFLT2] = { 
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_5V_NFLT2,
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_INPUTAD_VCU_SFTY_RESET] = { 
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_VCU_SFTY_RESET,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_INPUTAD_BSPD_MEM] = { 
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_BSPD_MEM,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
};

struct inputs_data {
    drv_mux_channel_S signal_mux;
};

struct inputs_data inputs_data = {
    .signal_mux = {
        .type = DRV_MUX_TYPE_GPIO,
        .config = {
            .max_output_channel = 2U,
            .outputs.gpio = {
                .pin_first = DRV_OUTPUTAD_MUX2_SEL1,
                .pin_last = DRV_OUTPUTAD_MUX2_SEL2,
            },
        },
    }
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
    drv_mux_init(&inputs_data.signal_mux);
    drv_mux_setMuxOutput(&inputs_data.signal_mux, 0U);
}

void drv_inputAD_1kHz_componentSpecific(void)
{
    HW_ADC_unpackADCBuffer();

    // This method only works since there is a 1:1 mapping from adc input to inputAD output
    for (uint8_t i = 0U; i < ADC_BANK1_CHANNEL_COUNT; i++)
    {
        drv_inputAD_private_setAnalogVoltage(i, HW_ADC_getVFromBank1Channel(i));
    }
    for (uint8_t i = 0U; i < ADC_BANK2_CHANNEL_COUNT; i++)
    {
        drv_inputAD_private_setAnalogVoltage(i + ADC_BANK1_CHANNEL_COUNT, HW_ADC_getVFromBank2Channel(i));
    }

    const float32_t hp_cs = HW_ADC_getVFromBank2Channel(ADC_BANK2_CHANNEL_MUX2_HP_CS);
    const float32_t thermistor_voltage = HW_ADC_getVFromBank2Channel(ADC_BANK2_CHANNEL_MUX2_THERMISTORS);

    switch (drv_mux_getMuxOutput(&inputs_data.signal_mux))
    {
        case 0U:
            drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_DEMUX2_PUMP, hp_cs);
            drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_DEMUX2_THERM_MCU, thermistor_voltage);
            break;
        case 1U:
            drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_DEMUX2_FAN, hp_cs);
            drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_DEMUX2_THERM_HSD1, thermistor_voltage);
            break;
        case 2U:
            drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_DEMUX2_THERM_HSD2, thermistor_voltage);
            break;
    }

    const uint8_t current_channel = drv_mux_getMuxOutput(&inputs_data.signal_mux);
    drv_mux_setMuxOutput(&inputs_data.signal_mux, (uint8_t)((current_channel + 1U) % 3U));

    drv_inputAD_private_runDigital();
}
