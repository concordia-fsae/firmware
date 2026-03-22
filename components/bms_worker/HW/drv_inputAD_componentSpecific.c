/**
 * @file IO.c
 * @brief  Source code for IO Module
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module Header */
#include "include/drv_inputAD_componentSpecific.h"
#include "drv_inputAD_private.h"

/**< System Includes*/
#include <string.h>

/**< Firmware Includes */
#include "HW.h"
#include "HW_adc.h"
#include "HW_dma.h"
#include "HW_gpio.h"
#include "HW_tim.h"
#include "drv_mux.h"
#include "BatteryMonitoring.h"

/**< Other Includes */
#include "ModuleDesc.h"
#include "lib_simpleFilter.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ADC_VOLTAGE_DIVISION    2.0f /**< Voltage division for cell voltage output */
#define SCALAR_VSNS_7V5         6.0f

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static drv_mux_channel_S signal_mux = {
    .type = DRV_MUX_TYPE_GPIO,
    .config = {
        .max_output_channel = 8U,
        .outputs.gpio = {
            .pin_first = DRV_OUTPUTAD_DIGITAL_MUX_SEL1,
            .pin_last = DRV_OUTPUTAD_DIGITAL_MUX_SEL3,
        },
    }
};

drv_inputAD_configDigital_S drv_inputAD_configDigital[DRV_INPUTAD_DIGITAL_COUNT] = {
#if APP_VARIANT_ID == 1
    [DRV_INPUTAD_DIGITAL_NSHUTDOWN] = {
        .type = INPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_NSHUTDOWN,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
#endif
};

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void drv_inputAD_private_unpackADCBufferCells(void);
void drv_inputAD_private_unpackADCBufferTemps(void);

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

    drv_mux_init(&signal_mux);
    drv_mux_setMuxOutput(&signal_mux, 0U);

    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_REF_VOLTAGE, ADC_REF_VOLTAGE);
}

static void drv_inputAD_1kHz_PRD(void)
{
    uint8_t current_sel = drv_mux_getMuxOutput(&signal_mux);

    HW_ADC_unpackADCBuffer();

    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_TEMP_MCU, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_TEMP_MCU));
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_TEMP_BALANCING1, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_TEMP_BALANCING1));
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_TEMP_BALANCING2, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_TEMP_BALANCING2));
#if APP_VARIANT_ID == 1U
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_TEMP_BOARD, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_TEMP_BOARD));
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_TEMP_THERM9, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_TEMP_THERM9));
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_VSNS_7V5, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_VSNS_7V5) * SCALAR_VSNS_7V5);
#endif
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_MUX1_CH1 + current_sel, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_MUX1));
#if APP_VARIANT_ID == 0U
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_MUX2_CH1 + current_sel, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_MUX2));
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_MUX3_CH1 + current_sel, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_MUX3));
#endif
    drv_inputAD_private_runDigital();

    drv_mux_setMuxOutput(&signal_mux, ++current_sel);

    if ((BMS.state == BMS_HOLDING) || (BMS.state == BMS_PARASITIC_MEASUREMENT))
    {
        const MAX_selectedCell_E current_cell = BMS_getCurrentOutputCell();

        if (current_cell == MAX_CELL1)
        {
            drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_CELL1 + current_cell,
                                                 HW_ADC_getVFromBank2Channel(ADC_BANK2_CHANNEL_BMS_CHIP) * ADC_VOLTAGE_DIVISION);
            BMS_measurementComplete();
        }
        else
        {
            if (BMS.delayed_measurement)
            {
                BMS.delayed_measurement = false;
            }
            else
            {
                BMS_setOutputCell(current_cell - 1);
                drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_CELL1 + current_cell,
                                                    HW_ADC_getVFromBank2Channel(ADC_BANK2_CHANNEL_BMS_CHIP) * ADC_VOLTAGE_DIVISION);
            }
        }
    }
    else if (BMS.state == BMS_SAMPLING)
    {
        drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_SEGMENT,
                                             HW_ADC_getVFromBank2Channel(ADC_BANK2_CHANNEL_BMS_CHIP) * ADC_VOLTAGE_DIVISION);
    }
}

/**
 * @brief  IO Module descriptor
 */
const ModuleDesc_S drv_inputAD_desc = {
    .moduleInit       = &drv_inputAD_init_componentSpecific,
    .periodic1kHz_CLK = &drv_inputAD_1kHz_PRD,
};
