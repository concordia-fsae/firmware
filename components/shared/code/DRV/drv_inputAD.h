/**
 * @file drv_inputAD.h
 * @brief  Header file for the digital and analog input driver
 *
 * Setup
 * 1. Define the digital and analog channels in drv_inputAD_componentSpecific.h
 *    and name them drv_inputAD_channelAnalog_E and drv_inputAD_getLogicLevel.
 * 2. Configure the digital channels in drv_inputAD_componentSpecific.c and name
 *    them drv_inputAD_configDigital
 * 3. Include drv_inputAD_private.h in drv_inputAD_componentSpecific.c and call
 *    the drv_inputAD_private_init function
 * 4. Periodically call the drv_inputAD_private_runDigital function to update
 *    the digital inputs and load new voltages with drv_inputAD_setAnalogVoltage
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_io.h"
#include "drv_inputAD_componentSpecific.h"
#include "HW_gpio.h"
#include "LIB_Types.h"
#include "CANTypes_generated.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    const enum
    {
        INPUT_DIGITAL,
        INPUT_DIGITAL_CAN,
    } type;
    const union
    {
        drv_io_pinConfig_S     gpio;
        CANRX_MESSAGE_health_E (*canrx_digitalStatus)(CAN_digitalStatus_E*);
    } config;
} drv_inputAD_configDigital_S;

typedef struct{
    drv_inputAD_channelAnalog_E channel; 
    float32_t voltage_divider_multiplier;
} drv_inputAD_configAnalog_S;

/******************************************************************************
 *       V O L T A G E   D I V I D E R   M U L T I P L I E R S
 ******************************************************************************/
//BMS boss analog channel multipliers
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CS                       16.0f 
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MCU_TEMP                 1.0f

//BMS worker analog channel multipliers
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX1_CH1                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX1_CH2                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX1_CH3                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX1_CH4                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX1_CH5                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX1_CH6                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX1_CH7                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX1_CH8                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX2_CH1                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX2_CH2                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX2_CH3                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX2_CH4                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX2_CH5                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX2_CH6                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX2_CH7                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX2_CH8                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX3_CH1                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX3_CH2                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX3_CH3                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX3_CH4                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX3_CH5                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX3_CH6                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX3_CH7                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX3_CH8                 1.0f
// All cell voltages must be sequential and ordered
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL1                    1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL2                    1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL3                    1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL4                    1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL5                    1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL6                    1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL7                    1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL8                    1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL9                    1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL10                   1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL11                   1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL12                   1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL13                   1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL14                   1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL15                   1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CELL16                   1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_SEGMENT                  2.0f     //ADC_VOLTAGE_DIVISION
#define DRV_INPUTAD_ANALOG_MULTIPLIER_BOARD_TEMP1              1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_BOARD_TEMP2              1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MCU_TEMP                 1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_REF_VOLTAGE              1.0f

//stw switch analog channel multipliers
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CHANNEL_AIN2             1.0f 
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CHANNEL_AIN4             1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CHANNEL_AIN6             1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CHANNEL_MCU_TEMP         1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CHANNEL_AIN1             1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CHANNEL_AIN3             1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CHANNEL_AIN5             1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_CHANNEL_AIN7             1.0f

//VC front analog channel multipliers 
#define DRV_INPUTAD_ANALOG_MULTIPLIER_R_BR_TEMP                1.0f            
#define DRV_INPUTAD_ANALOG_MULTIPLIER_L_SHK_DISP               1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_PU1                      1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_BR_POT                   1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_SPARE1                   1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_STR_ANGLE                1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_APPS_P1                  1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_L_BR_TEMP                1.0f            
#define DRV_INPUTAD_ANALOG_MULTIPLIER_R_SHK_DISP               1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_PU2                      1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_BR_PR                    1.681f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_SPARE3                   1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_SPARE4                   1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_APPS_P2                  1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_BOARD_TEMP               1.0f
//VC rear analog channel multipliers
#define DRV_INPUTAD_ANALOG_MULTIPLIER_SPARE2                   1.0f

//VC pdu analog channel multipliers 
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX_LP1_SNS              1.0f            
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX_LP2_SNS              1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX_LP3_SNS              1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX_LP4_SNS              1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_5V_VOLTAGE               3.61f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_UVL_BATT                 6.62f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX_LP5_SNS              1.0f            
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX_LP6_SNS              1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX_LP7_SNS              1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX_LP8_SNS              1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX_LP9_SNS              1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX2_HP_CS               1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_MUX2_THERMISTORS         1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_DEMUX2_PUMP              1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_DEMUX2_FAN               1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_DEMUX2_5V_SNS            0.20f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_DEMUX2_THERM_MCU         1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_DEMUX2_THERM_HSD1        1.0f
#define DRV_INPUTAD_ANALOG_MULTIPLIER_DEMUX2_THERM_HSD2        1.0f

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t            drv_inputAD_getAnalogVoltage(drv_inputAD_channelAnalog_E channel);
drv_io_logicLevel_E  drv_inputAD_getLogicLevel(drv_inputAD_channelDigital_E channel);
drv_io_activeState_E drv_inputAD_getDigitalActiveState(drv_inputAD_channelDigital_E channel);
