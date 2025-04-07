/**
 * @file drv_tempSensors.h
 * @brief  Header file for the temperature sensor driver
 *
 * Setup
 * 1. Define drv_tempSensors_channel_E in drv_tempSensor_componentSpecific.h
 * 2. Configure all the temperature sensors in drv_tempSensor_componentSpecific.c
 *    and name the array drv_tempSensors_channels
 * 3. Periodically retrieve the temperature from the channel with
 *    drv_tempSensors_getChannelTemperatureDegC
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_tempSensors_componentSpecific.h"
#include "lib_thermistors.h"
#include "drv_inputAD.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

/**
 * @brief The type of temperature sensor
 * @value DRV_TEMPSENSORS_SENSOR_THERMISTOR_LOWSIDE A thermistor on the low side of
 *        a voltage divider
 * @value DRV_TEMPSENSORS_SENSOR_LINEAR A linear temperature sensor such as a diode
 * @value DRV_TEMPSENSORS_SENSOR_FUNC A function that returns a value in degrees C
 */
typedef enum
{
    DRV_TEMPSENSORS_SENSOR_THERMISTOR_LOWSIDE,
    DRV_TEMPSENSORS_SENSOR_LINEAR,
    DRV_TEMPSENSORS_SENSOR_FUNC,
} drv_tempSensors_sensorType_E;

typedef struct
{
    lib_thermistors_BParameter_S const * const b_param;
    const float32_t                            fixed_resistance;
    const drv_inputAD_channelAnalog_E          adc_channel;
    const drv_inputAD_channelAnalog_E          ref_voltage;
} drv_tempSensors_configThermistorLowSide_S;

typedef struct
{
    const float32_t                   t0_temp;
    const float32_t                   t0_voltage;
    const float32_t                   degC_per_mV;
    const drv_inputAD_channelAnalog_E adc_channel;
} drv_tempSensors_configLinear_S;

typedef union
{
    drv_tempSensors_configThermistorLowSide_S thermistor_ls;
    drv_tempSensors_configLinear_S            linear;
    float32_t                                 (*func)(void);
} drv_tempSensors_config_S;

typedef struct
{
    const drv_tempSensors_sensorType_E sensor_type;
    drv_tempSensors_config_S           config;
} drv_tempSensors_channelConfig_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t drv_tempSensors_getThermistorLSTemperatureDegC(drv_tempSensors_configThermistorLowSide_S const * channel);
float32_t drv_tempSensors_getLinearTemperatureDegC(drv_tempSensors_configLinear_S const * channel);
float32_t drv_tempSensors_getChannelTemperatureDegC(drv_tempSensors_channel_E channel);
