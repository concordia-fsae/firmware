/**
 * @file drv_inputAD.c
 * @brief  Source file for the digital and analog input driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_inputAD.h"
#include "drv_inputAD_private.h"
#include <string.h>
#include "HW_gpio.h"
#include "HW_adc.h"
#include "drv_io.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern drv_inputAD_configDigital_S drv_inputAD_configDigital[DRV_INPUTAD_DIGITAL_COUNT];

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t           voltages[DRV_INPUTAD_ANALOG_COUNT];
    drv_io_logicLevel_E logic_levels[DRV_INPUTAD_DIGITAL_COUNT];
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
    switch (drv_inputAD_configDigital[channel].type)
    {
        case INPUT_DIGITAL:
            inputs.logic_levels[channel] = HW_GPIO_readPin(drv_inputAD_configDigital[channel].config.gpio.pin) ? DRV_IO_LOGIC_HIGH : DRV_IO_LOGIC_LOW;
            break;
        case INPUT_DIGITAL_CAN:
            {
                CAN_digitalStatus_E status = CAN_DIGITALSTATUS_OFF;
                if (drv_inputAD_configDigital[channel].config.canrx_digitalStatus(&status) == CANRX_MESSAGE_VALID)
                {
                    inputs.logic_levels[channel] = (status == CAN_DIGITALSTATUS_ON) ? DRV_IO_LOGIC_HIGH : DRV_IO_LOGIC_LOW;
                }
            }
            break;
        default:
            break;
    }
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
void drv_inputAD_private_setRawAnalogVoltage(drv_inputAD_channelAnalog_E channel, float32_t voltage)
{
    //apply voltage divider multiplier from configuration
    float32_t divided_voltage = voltage * drv_inputAD_configAnalog[channel].multiplier;
    inputs.voltages[channel] = divided_voltage;
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
drv_io_logicLevel_E drv_inputAD_getLogicLevel(drv_inputAD_channelDigital_E channel)
{
    inputAD_getDigitalInput(channel);
    return inputs.logic_levels[channel];
}

/**
 * @brief Get the current active state of the input
 * @param channel The channel to retrieve
 * @return The current active state of the channel
 */
drv_io_activeState_E drv_inputAD_getDigitalActiveState(drv_inputAD_channelDigital_E channel)
{
    drv_io_activeState_E ret = DRV_IO_INACTIVE;

    switch (drv_inputAD_configDigital[channel].type)
    {
        case INPUT_DIGITAL:
            ret = (drv_inputAD_getLogicLevel(channel) == drv_inputAD_configDigital[channel].config.gpio.active_level) ?
                   DRV_IO_ACTIVE :
                   DRV_IO_INACTIVE;
            break;
        case INPUT_DIGITAL_CAN:
            ret = (drv_inputAD_getLogicLevel(channel) == DRV_IO_LOGIC_HIGH) ? DRV_IO_ACTIVE : DRV_IO_INACTIVE;
            break;
        default:
            break;
    }

    return ret;
}
