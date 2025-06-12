/**
 * @file drv_outputAD.c
 * @brief  Source file for the digital and analog output driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_outputAD.h"
#include "HW_gpio.h"
#include "drv_io.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern drv_outputAD_configDigital_S drv_outputAD_configDigital[DRV_OUTPUTAD_DIGITAL_COUNT];

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t voltages[DRV_OUTPUTAD_ANALOG_COUNT];
    struct
    {
        drv_io_activeState_E active_state;
    } digital[DRV_OUTPUTAD_DIGITAL_COUNT];
} outputAD_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static outputAD_S outputs;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Output the state to the physical interface
 * @param channel Channel to output
 */
static void setDigitalOutputState(drv_outputAD_channelDigital_E channel)
{
    const drv_io_logicLevel_E desired_level = (outputs.digital[channel].active_state == DRV_IO_ACTIVE) ?
                                               drv_outputAD_configDigital[channel].config.gpio.active_level :
                                               drv_io_invertLogicLevel(drv_outputAD_configDigital[channel].config.gpio.active_level);
    const bool state_to_set = desired_level == DRV_IO_LOGIC_HIGH;

    HW_GPIO_writePin(drv_outputAD_configDigital[channel].config.gpio.pin, state_to_set);
}

/**
 * @brief Output the voltage to the physical interface
 * @param channel Channel to output
 */
static void setAnalogOutputVoltage(drv_outputAD_channelAnalog_E channel)
{
    (void)channel;
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Set the analog output voltage of a channel
 * @param channel The channel to output
 * @param voltage The voltage to output
 */
void drv_outputAD_setAnalogVoltage(drv_outputAD_channelAnalog_E channel, float32_t voltage)
{
    outputs.voltages[channel] = voltage;
    setAnalogOutputVoltage(channel);
}

/**
 * @brief Set a digital output's active state
 * @param channel The channel to output
 * @param state The state to output
 */
void drv_outputAD_setDigitalActiveState(drv_outputAD_channelDigital_E channel, drv_io_activeState_E state)
{
    outputs.digital[channel].active_state = state;
    setDigitalOutputState(channel);
}

/**
 * @brief Toggle the digital state of an output
 * @param channel The output to invert
 */
void drv_outputAD_toggleDigitalState(drv_outputAD_channelDigital_E channel)
{
    outputs.digital[channel].active_state = (outputs.digital[channel].active_state == DRV_IO_ACTIVE) ?
                                             DRV_IO_INACTIVE :
                                             DRV_IO_ACTIVE;
    setDigitalOutputState(channel);
}

/**
 * @brief Get the output voltage of an analog channel
 * @param channel The channel to retrieve
 * @return The current voltage being output
 */
float32_t drv_outputAD_getAnalogVoltage(drv_outputAD_channelAnalog_E channel)
{
    return outputs.voltages[channel];
}

/**
 * @brief Get the output state of a digital channel
 * @param channel The channel to retrieve
 * @return The current state being output
 */
drv_io_activeState_E drv_outputAD_getDigitalActiveState(drv_outputAD_channelDigital_E channel)
{
    return outputs.digital[channel].active_state;
}
