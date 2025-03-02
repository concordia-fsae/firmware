/**
 * @file DRV_inputAD.h
 * @brief  Header file for the digital and analog input driver
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "DRV_inputAD.h"
#include <string.h>
#include "HW_gpio.h"
#include "HW_adc.h"
#include "LIB_simpleFilter.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern DRV_inputAD_configDigital_S DRV_inputAD_configDigital[DRV_INPUTAD_DIGITAL_COUNT];

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    LIB_simpleFilter_s       voltages[DRV_INPUTAD_ANALOG_COUNT];
    DRV_inputAD_logicLevel_E logic_levels[DRV_INPUTAD_DIGITAL_COUNT];
} inputAD_S;

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static void inputAD_readDigitalInputs(void);

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static inputAD_S inputs;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void inputAD_getDigitalInputs(void)
{
    for(uint8_t i = 0U; i < DRV_INPUTAD_DIGITAL_COUNT; i++)
    {
        inputs.logic_levels[i] = HW_gpio_readPin(DRV_inputAD_configDigital[i].pin) ? DRV_INPUTAD_LOGIC_HIGH : DRV_INPUTAD_LOGIC_LOW;
    }
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void DRV_inputAD_init(void)
{
    memset(&inputs, 0x00U, sizeof(inputs));

    inputAD_getDigitalInputs();
}

float32_t DRV_inputAD_getAnalogVoltage(DRV_inputAD_channelAnalog_E channel)
{
    return inputs.voltages[channel].value;
}

DRV_inputAD_logicLevel_E DRV_inputAD_getLogicLevel(DRV_inputAD_channelDigital_E channel)
{
    return inputs.logic_levels[channel];
}

