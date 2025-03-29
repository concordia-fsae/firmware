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
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint32_t           adcBuffer[DRV_INPUTAD_ADC_BUF_LEN];
    LIB_simpleFilter_S adcData_bank1[ADC_BANK1_CHANNEL_COUNT];
    LIB_simpleFilter_S adcData_bank2[ADC_BANK2_CHANNEL_COUNT];
} io_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static io_S io;

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
    memset(&io, 0x00, sizeof(io));

    drv_inputAD_private_init();

    NX3L_init();
    NX3L_enableMux();
    NX3L_setMux(NX3L_MUX1);

    HW_ADC_startDMA(&hadc1, (uint32_t*)&io.adcBuffer, DRV_INPUTAD_ADC_BUF_LEN);
}

static void drv_inputAD_10Hz_PRD(void)
{
    static NX3L_MUXChannel_E current_sel = NX3L_MUX1;

    drv_inputAD_private_unpackADCBufferTemps();

    for (uint8_t i = 0; i < ADC_BANK1_CHANNEL_COUNT; i++)
    {
        io.adcData_bank1[i].value = HW_ADC_getVFromCount((uint16_t)io.adcData_bank1[i].value);
    }

    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_MCU_TEMP, io.adcData_bank1[ADC_BANK1_CHANNEL_MCU_TEMP].value);
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_BOARD_TEMP1, io.adcData_bank1[ADC_BANK1_CHANNEL_BOARD1].value);
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_BOARD_TEMP2, io.adcData_bank1[ADC_BANK1_CHANNEL_BOARD2].value);
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_MUX1_CH1 + current_sel, io.adcData_bank1[ADC_BANK1_CHANNEL_MUX1].value);
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_MUX2_CH1 + current_sel, io.adcData_bank1[ADC_BANK1_CHANNEL_MUX2].value);
    drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_MUX3_CH1 + current_sel, io.adcData_bank1[ADC_BANK1_CHANNEL_MUX3].value);

    if (++current_sel == NX3L_MUX_COUNT)
    {
        current_sel = NX3L_MUX1;
    }

    NX3L_setMux(current_sel);
}

#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK
/**
 * @brief  10kHz IO periodic function
 */
static void IO10kHz_PRD(void)
#else // FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK
/**
 * @brief  10kHz IO periodic function
 */
void IO10kHz_CB(void)
#endif // not FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK
{
    if ((BMS.state == BMS_HOLDING) || (BMS.state == BMS_PARASITIC_MEASUREMENT))
    {
        const MAX_selectedCell_E current_cell = BMS_getCurrentOutputCell();

        drv_inputAD_private_unpackADCBufferCells();

        io.adcData_bank2[ADC_BANK2_CHANNEL_BMS_CHIP].value = HW_ADC_getVFromCount((uint16_t)io.adcData_bank2[ADC_BANK2_CHANNEL_BMS_CHIP].value) * ADC_VOLTAGE_DIVISION;

        drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_CELL1 + current_cell, io.adcData_bank2[ADC_BANK2_CHANNEL_BMS_CHIP].value);

        if (current_cell == MAX_CELL1)
        {
            BMS_measurementComplete();
        }
        else
        {
            BMS_setOutputCell(current_cell - 1);
#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
            HW_TIM_10kHz_timerStart();
#endif // FEATUFEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
        }
    }
    else if (BMS.state == BMS_SAMPLING)
    {
        drv_inputAD_private_unpackADCBufferCells();
        io.adcData_bank2[ADC_BANK2_CHANNEL_BMS_CHIP].value = HW_ADC_getVFromCount((uint16_t)io.adcData_bank2[ADC_BANK2_CHANNEL_BMS_CHIP].value) * ADC_VOLTAGE_DIVISION;
        drv_inputAD_private_setAnalogVoltage(DRV_INPUTAD_ANALOG_SEGMENT, io.adcData_bank2[ADC_BANK2_CHANNEL_BMS_CHIP].value);
    }
}

/**
 * @brief  IO Module descriptor
 */
const ModuleDesc_S IO_desc = {
    .moduleInit       = &drv_inputAD_init_componentSpecific,
    .periodic10Hz_CLK = &drv_inputAD_10Hz_PRD,
#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK
    .periodic10kHz_CLK = &IO10kHz_PRD,
#endif // FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Unpack ADC buffer for the thermistor measurements
 */
void drv_inputAD_private_unpackADCBufferTemps(void)
{
    for (uint8_t i = 0; i < ADC_BANK1_CHANNEL_COUNT; i++)
    {
        LIB_simpleFilter_clear(&io.adcData_bank1[i]);
    }

    for (uint16_t i = 0; i < DRV_INPUTAD_ADC_BUF_LEN; i++)
    {
        LIB_simpleFilter_increment(&io.adcData_bank1[i % ADC_BANK1_CHANNEL_COUNT], (io.adcBuffer[i] & 0xffff));
    }

    for (uint8_t i = 0; i < ADC_BANK1_CHANNEL_COUNT; i++)
    {
        LIB_simpleFilter_average(&io.adcData_bank1[i]);
    }
}

/**
 * @brief  Unpack ADC buffer for the cell measurements
 */
void drv_inputAD_private_unpackADCBufferCells(void)
{
    LIB_simpleFilter_clear(&io.adcData_bank2[ADC_BANK2_CHANNEL_BMS_CHIP]);

    for (uint16_t i = 0; i < DRV_INPUTAD_ADC_BUF_LEN; i++)
    {
        LIB_simpleFilter_increment(&io.adcData_bank2[ADC_BANK2_CHANNEL_BMS_CHIP], (io.adcBuffer[i] >> 16U));
    }

    LIB_simpleFilter_average(&io.adcData_bank2[ADC_BANK2_CHANNEL_BMS_CHIP]);
}
