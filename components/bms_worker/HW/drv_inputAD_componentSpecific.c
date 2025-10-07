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
#include "HW_tim.h"
#include "HW_NX3L4051PW.h"
#include "BatteryMonitoring.h"

/**< Other Includes */
#include "ModuleDesc.h"
#include "LIB_simpleFilter.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ADC_VOLTAGE_DIVISION    2.0f /**< Voltage division for cell voltage output */

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

    NX3L_init();
    NX3L_enableMux();
    NX3L_setMux(NX3L_MUX1);

    drv_inputAD_private_setRawAnalogVoltage(DRV_INPUTAD_ANALOG_REF_VOLTAGE, ADC_REF_VOLTAGE);
}

static void drv_inputAD_1kHz_PRD(void)
{
    static NX3L_MUXChannel_E current_sel = NX3L_MUX1;

    HW_ADC_unpackADCBuffer();

    drv_inputAD_private_setRawAnalogVoltage(DRV_INPUTAD_ANALOG_MCU_TEMP, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_MCU_TEMP));
    drv_inputAD_private_setRawAnalogVoltage(DRV_INPUTAD_ANALOG_BOARD_TEMP1, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_BOARD1));
    drv_inputAD_private_setRawAnalogVoltage(DRV_INPUTAD_ANALOG_BOARD_TEMP2, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_BOARD2));
    drv_inputAD_private_setRawAnalogVoltage(DRV_INPUTAD_ANALOG_MUX1_CH1 + current_sel, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_MUX1));
    drv_inputAD_private_setRawAnalogVoltage(DRV_INPUTAD_ANALOG_MUX2_CH1 + current_sel, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_MUX2));
    drv_inputAD_private_setRawAnalogVoltage(DRV_INPUTAD_ANALOG_MUX3_CH1 + current_sel, HW_ADC_getVFromBank1Channel(ADC_BANK1_CHANNEL_MUX3));

    if (++current_sel == NX3L_MUX_COUNT)
    {
        current_sel = NX3L_MUX1;
    }

    NX3L_setMux(current_sel);

    if ((BMS.state == BMS_HOLDING) || (BMS.state == BMS_PARASITIC_MEASUREMENT))
    {
        const MAX_selectedCell_E current_cell = BMS_getCurrentOutputCell();

        if (current_cell == MAX_CELL1)
        {
            drv_inputAD_private_setRawAnalogVoltage(DRV_INPUTAD_ANALOG_CELL1 + current_cell,
                                                 HW_ADC_getVFromBank2Channel(ADC_BANK2_CHANNEL_BMS_CHIP));
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
                drv_inputAD_private_setRawAnalogVoltage(DRV_INPUTAD_ANALOG_CELL1 + current_cell,
                                                    HW_ADC_getVFromBank2Channel(ADC_BANK2_CHANNEL_BMS_CHIP));
            }
        }
    }
    else if (BMS.state == BMS_SAMPLING)
    {
        drv_inputAD_private_setRawAnalogVoltage(DRV_INPUTAD_ANALOG_SEGMENT,
                                             HW_ADC_getVFromBank2Channel(ADC_BANK2_CHANNEL_BMS_CHIP));
    }
}

/**
 * @brief  IO Module descriptor
 */
const ModuleDesc_S drv_inputAD_desc = {
    .moduleInit       = &drv_inputAD_init_componentSpecific,
    .periodic1kHz_CLK = &drv_inputAD_1kHz_PRD,
};
