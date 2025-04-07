/**
 * @file drv_tempSensors.c
 * @brief  Source file for the temperature sensor driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_tempSensors.h"
#include "lib_voltageDivider.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern drv_tempSensors_channelConfig_S drv_tempSensors_channels[DRV_TEMPSENSORS_CHANNEL_COUNT];

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * @brief Get the temperature of a thermistor on the low side of a voltage divider
 * @param channel The pointer to a  low side thermistor struct 
 * @return The current temperature in degrees celsius
 */
float32_t drv_tempSensors_getThermistorLSTemperatureDegC(drv_tempSensors_configThermistorLowSide_S const * channel)
{
    const float32_t channel_voltage = drv_inputAD_getAnalogVoltage(channel->adc_channel);
    const float32_t therm_resistance = lib_voltageDivider_getRFromVKnownPullUp(channel_voltage,
                                                                               channel->fixed_resistance,
                                                                               drv_inputAD_getAnalogVoltage(channel->ref_voltage));
    return lib_thermistors_getCelsiusFromR_BParameter(channel->b_param,
                                                      therm_resistance);

}

/**
 * @brief Get the temperature of a linear temperature sensor
 * @param channel The pointer to a linear temp sensor struct
 * @return The current temperature in degrees celsius
 */
float32_t drv_tempSensors_getLinearTemperatureDegC(drv_tempSensors_configLinear_S const * channel)
{
    const float32_t channel_voltage = drv_inputAD_getAnalogVoltage(channel->adc_channel);
    return channel->t0_temp + (((channel_voltage - channel->t0_voltage) * channel->degC_per_mV) * 1000.0f);
}

/**
 * @brief Get the temperature of a temperature sensor channel
 * @param The channel to retrieve
 * @return The current temperature in degrees celsius
 */
float32_t drv_tempSensors_getChannelTemperatureDegC(drv_tempSensors_channel_E channel)
{
    float32_t ret = 0.0f;

    switch (drv_tempSensors_channels[channel].sensor_type)
    {
        case DRV_TEMPSENSORS_SENSOR_THERMISTOR_LOWSIDE:
            ret = drv_tempSensors_getThermistorLSTemperatureDegC(&drv_tempSensors_channels[channel].config.thermistor_ls);
            break;
        case DRV_TEMPSENSORS_SENSOR_LINEAR:
            ret = drv_tempSensors_getLinearTemperatureDegC(&drv_tempSensors_channels[channel].config.linear);
            break;
        case DRV_TEMPSENSORS_SENSOR_FUNC:
            ret = drv_tempSensors_channels[channel].config.func();
    }

    return ret;
}
