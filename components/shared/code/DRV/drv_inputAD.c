/**
 * @file drv_inputAD.c
 * @brief  Source file for the digital and analog input driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_inputAD_private.h"
#include <string.h>
#include "HW_gpio.h"
#include "HW_adc.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern drv_inputAD_configDigital_S drv_inputAD_configDigital[DRV_INPUTAD_DIGITAL_COUNT];

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t                voltages[DRV_INPUTAD_ANALOG_COUNT];
    drv_inputAD_logicLevel_E logic_levels[DRV_INPUTAD_DIGITAL_COUNT];
} inputAD_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static inputAD_S inputs;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Populate the most recent pin state
 * @param channel Digital channel to update
 */
static void inputAD_getDigitalInput(drv_inputAD_channelDigital_E channel)
{
    inputs.logic_levels[channel] = HW_GPIO_readPin(drv_inputAD_configDigital[channel].pin) ? DRV_INPUTAD_LOGIC_HIGH : DRV_INPUTAD_LOGIC_LOW;
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Initialize inputAD
 */
void drv_inputAD_private_init(void)
{
    memset(&inputs, 0x00U, sizeof(inputs));
}

/**
 * @brief Update digital pin states
 */
void drv_inputAD_private_runDigital(void)
{
    for(uint8_t i = 0U; i < DRV_INPUTAD_DIGITAL_COUNT; i++)
    {
        inputAD_getDigitalInput(i);
    }
}

/**
 * @brief Update the analog voltage of a pin
 * @param channel Analog channel to update
 * @param voltage Measured voltage
 */
void drv_inputAD_private_setAnalogVoltage(drv_inputAD_channelAnalog_E channel, float32_t voltage)
{
    inputs.voltages[channel] = voltage;
}

/**
 * @breif Get the last measured analog voltage of a pin
 * @param channel Analog channel to retrieve
 * @returns Last measured voltage
 */
float32_t drv_inputAD_getAnalogVoltage(drv_inputAD_channelAnalog_E channel)
{
    return inputs.voltages[channel];
}

/**
 * @brief Get the logic level of a digital pin
 * @param channel Channel to retrieve
 * @return Current logic level state
 */
drv_inputAD_logicLevel_E drv_inputAD_getLogicLevel(drv_inputAD_channelDigital_E channel)
{
    inputAD_getDigitalInput(channel);
    return inputs.logic_levels[channel];
}

