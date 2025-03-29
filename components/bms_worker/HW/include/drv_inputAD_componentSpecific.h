/**
 * @file IO.h
 * @brief  Header file for IO Module
 */

#pragma once

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define DRV_INPUTAD_ADC_BUF_LEN 48U // processed when half full and again when completely full
                                    // (13.5+3) cycles * 48 samples @ 8MHz -> 99us < 200us - 15us

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_INPUTAD_DIGITAL_COUNT,
} drv_inputAD_channelDigital_E;

typedef enum
{
    // All mux channels must be sequential and ordered
    DRV_INPUTAD_ANALOG_MUX1_CH1 = 0x00U,
    DRV_INPUTAD_ANALOG_MUX1_CH2,
    DRV_INPUTAD_ANALOG_MUX1_CH3,
    DRV_INPUTAD_ANALOG_MUX1_CH4,
    DRV_INPUTAD_ANALOG_MUX1_CH5,
    DRV_INPUTAD_ANALOG_MUX1_CH6,
    DRV_INPUTAD_ANALOG_MUX1_CH7,
    DRV_INPUTAD_ANALOG_MUX1_CH8,
    DRV_INPUTAD_ANALOG_MUX2_CH1,
    DRV_INPUTAD_ANALOG_MUX2_CH2,
    DRV_INPUTAD_ANALOG_MUX2_CH3,
    DRV_INPUTAD_ANALOG_MUX2_CH4,
    DRV_INPUTAD_ANALOG_MUX2_CH5,
    DRV_INPUTAD_ANALOG_MUX2_CH6,
    DRV_INPUTAD_ANALOG_MUX2_CH7,
    DRV_INPUTAD_ANALOG_MUX2_CH8,
    DRV_INPUTAD_ANALOG_MUX3_CH1,
    DRV_INPUTAD_ANALOG_MUX3_CH2,
    DRV_INPUTAD_ANALOG_MUX3_CH3,
    DRV_INPUTAD_ANALOG_MUX3_CH4,
    DRV_INPUTAD_ANALOG_MUX3_CH5,
    DRV_INPUTAD_ANALOG_MUX3_CH6,
    DRV_INPUTAD_ANALOG_MUX3_CH7,
    DRV_INPUTAD_ANALOG_MUX3_CH8,
    // All cell voltages must be sequential and ordered
    DRV_INPUTAD_ANALOG_CELL1,
    DRV_INPUTAD_ANALOG_CELL2,
    DRV_INPUTAD_ANALOG_CELL3,
    DRV_INPUTAD_ANALOG_CELL4,
    DRV_INPUTAD_ANALOG_CELL5,
    DRV_INPUTAD_ANALOG_CELL6,
    DRV_INPUTAD_ANALOG_CELL7,
    DRV_INPUTAD_ANALOG_CELL8,
    DRV_INPUTAD_ANALOG_CELL9,
    DRV_INPUTAD_ANALOG_CELL10,
    DRV_INPUTAD_ANALOG_CELL11,
    DRV_INPUTAD_ANALOG_CELL12,
    DRV_INPUTAD_ANALOG_CELL13,
    DRV_INPUTAD_ANALOG_CELL14,
    DRV_INPUTAD_ANALOG_CELL15,
    DRV_INPUTAD_ANALOG_CELL16,
    DRV_INPUTAD_ANALOG_SEGMENT,
    DRV_INPUTAD_ANALOG_BOARD_TEMP1,
    DRV_INPUTAD_ANALOG_BOARD_TEMP2,
    DRV_INPUTAD_ANALOG_MCU_TEMP,
    DRV_INPUTAD_ANALOG_COUNT,
} drv_inputAD_channelAnalog_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
void IO10kHz_CB(void);
#endif // not FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK
