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
    [DRV_TEMPSENSORS_CHANNEL_MCU_TEMP] = {
        .sensor_type = DRV_TEMPSENSORS_SENSOR_LINEAR,
        .config.linear = {
            .t0_temp = 25,
            .t0_voltage = 1.43f,
            .degC_per_mV = 0.0043f,
            .adc_channel = DRV_INPUTAD_ANALOG_MCU_TEMP,
        },
    },
    [DRV_TEMPSENSORS_CHANNEL_BOARD1] = {
        .sensor_type = DRV_TEMPSENSORS_SENSOR_THERMISTOR_LOWSIDE,
        .config.thermistor_ls = {
            .b_param = &NCP21_bParam,
            .fixed_resistance = 10000,
            .adc_channel = DRV_INPUTAD_ANALOG_BOARD_TEMP1,
            .ref_voltage = DRV_INPUTAD_ANALOG_REF_VOLTAGE,
        },
    },
    [DRV_TEMPSENSORS_CHANNEL_BOARD2] = {
        .sensor_type = DRV_TEMPSENSORS_SENSOR_THERMISTOR_LOWSIDE,
        .config.thermistor_ls = {
            .b_param = &NCP21_bParam,
            .fixed_resistance = 10000,
            .adc_channel = DRV_INPUTAD_ANALOG_BOARD_TEMP1,
            .ref_voltage = DRV_INPUTAD_ANALOG_REF_VOLTAGE,
        },
    },
};
