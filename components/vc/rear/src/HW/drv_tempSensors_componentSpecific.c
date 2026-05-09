/**
 * @file drv_tempSensors_componentSpecific.c
 * @brief  Source file for the component specific temperature sensor driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_tempSensors.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

drv_tempSensors_channelConfig_S drv_tempSensors_channels[] = {
    [DRV_TEMPSENSORS_CHANNEL_TS_CAP] = {
        .sensor_type = DRV_TEMPSENSORS_SENSOR_THERMISTOR_LOWSIDE,
        .config.thermistor_ls = {
            .b_param = &NTC103JT_bParam,
            .fixed_resistance = 1000,
            .adc_channel = DRV_INPUTAD_ANALOG_PU1,
            .ref_voltage = DRV_INPUTAD_ANALOG_REF_VOLTAGE,
        },
    },
};
