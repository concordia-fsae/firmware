/**
 * @file drv_tempSensors_componentSpecific.c
 * @brief Header file for the VCREAR component specific temperature sensors
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_tempSensors.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

drv_tempSensors_channelConfig_S drv_tempSensors_channels[DRV_TEMPSENSORS_CHANNEL_COUNT] = {
    [DRV_TEMPSENSOR_L_BR_TEMP] = {
        .sensor_type = DRV_TEMPSENSORS_SENSOR_LINEAR,
        .config = {
            .linear = {
        .t0_temp = 0.0f,
        .t0_voltage = 0.5f,
        .degC_per_mV = 0.2f,
        .adc_channel = DRV_INPUTAD_ANALOG_L_BR_TEMP,
            },
        },
    },
    [DRV_TEMPSENSOR_R_BR_TEMP] = {
        .sensor_type = DRV_TEMPSENSORS_SENSOR_LINEAR,
        .config = {
            .linear = {
        .t0_temp = 0.0f,
        .t0_voltage = 0.5f,
        .degC_per_mV = 5.0f,
        .adc_channel = DRV_INPUTAD_ANALOG_R_BR_TEMP,
            },
        },
    },
};
