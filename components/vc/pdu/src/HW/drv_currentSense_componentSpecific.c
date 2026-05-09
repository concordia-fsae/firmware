/**
 * @file drv_currentSense_componentSpecific.c
 * @brief  Component specific source file for current sensing driver on the PDU board. Includes current sensing for powerManager.c, drv_tps2hb16ab.c and drv_vn90008.c
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_currentSense.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define AMP_PER_VOLT_VN9008        7.518f
#define AMP_PER_VOLT_TPS2HB16AB    1.2048f

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

drv_currentSense_S drv_currentSense_spec[DRV_CURRENTSENSE_CHANNEL_COUNT] =
{
// ------ PDU 5V ------

    [DRV_CURRENTSENSE_CHANNEL_PDU_5V] =    // channel 0 = powerManager.c
    {
    .adc_channel  = DRV_INPUTAD_ANALOG_DEMUX2_5V_SNS,
    .amp_per_volt =                            0.20f,
    .offset       =                             0.0f,
    },


// ------ VN9008 ------

    [DRV_CURRENTSENSE_CHANNEL_PUMP] =    // channel 1 = drv_vn9008.c
    {
    .adc_channel  = DRV_INPUTAD_ANALOG_DEMUX2_PUMP,
    .amp_per_volt = AMP_PER_VOLT_VN9008,
    .offset       =                           0.0f,
    },


    [DRV_CURRENTSENSE_CHANNEL_FAN] =    // channel 2 = drv_vn9008.c
    {
    .adc_channel  = DRV_INPUTAD_ANALOG_DEMUX2_FAN,
    .amp_per_volt = AMP_PER_VOLT_VN9008,
    .offset       =                          0.0f,
    },


// ------ tps2hb16ab ------

    [DRV_CURRENTSENSE_CHANNEL_BMS1_SHUTDOWN] =    // channel 3 = drv_tps2hb16ab.c
    {
    .adc_channel  = DRV_INPUTAD_ANALOG_MUX_LP1_SNS,
    .amp_per_volt = AMP_PER_VOLT_TPS2HB16AB,
    .offset       =                           0.0f,
    },


    [DRV_CURRENTSENSE_CHANNEL_BMS2_ACCUM] =    // channel 4 = drv_tps2hb16ab.c
    {
    .adc_channel  = DRV_INPUTAD_ANALOG_MUX_LP2_SNS,
    .amp_per_volt = AMP_PER_VOLT_TPS2HB16AB,
    .offset       =                           0.0f,
    },


    [DRV_CURRENTSENSE_CHANNEL_BMS3_SENSOR] =    // channel 5 = drv_tps2hb16ab.c
    {
    .adc_channel  = DRV_INPUTAD_ANALOG_MUX_LP3_SNS,
    .amp_per_volt = AMP_PER_VOLT_TPS2HB16AB,
    .offset       =                           0.0f,
    },


    [DRV_CURRENTSENSE_CHANNEL_VC1_VC2] =    // channel 6 = drv_tps2hb16ab.c
    {
    .adc_channel  = DRV_INPUTAD_ANALOG_MUX_LP4_SNS,
    .amp_per_volt = AMP_PER_VOLT_TPS2HB16AB,
    .offset       =                           0.0f,
    },


    [DRV_CURRENTSENSE_CHANNEL_MC_VCU3] =    // channel 7 = drv_tps2hb16ab.c
    {
    .adc_channel  = DRV_INPUTAD_ANALOG_MUX_LP5_SNS,
    .amp_per_volt = AMP_PER_VOLT_TPS2HB16AB,
    .offset       =                           0.0f,
    },


    [DRV_CURRENTSENSE_CHANNEL_HVE_COCKPIT] =    // channel 8 = drv_tps2hb16ab.c
    {
    .adc_channel  = DRV_INPUTAD_ANALOG_MUX_LP6_SNS,
    .amp_per_volt = AMP_PER_VOLT_TPS2HB16AB,
    .offset       =                           0.0f,
    },


    [DRV_CURRENTSENSE_CHANNEL_SPARE_BMS4] =    // channel 9 = drv_tps2hb16ab.c
    {
    .adc_channel  = DRV_INPUTAD_ANALOG_MUX_LP7_SNS,
    .amp_per_volt = AMP_PER_VOLT_TPS2HB16AB,
    .offset       =                           0.0f,
    },


    [DRV_CURRENTSENSE_CHANNEL_VCU1_VCU2] =    // channel 10 = drv_tps2hb16ab.c
    {
    .adc_channel  = DRV_INPUTAD_ANALOG_MUX_LP8_SNS,
    .amp_per_volt = AMP_PER_VOLT_TPS2HB16AB,
    .offset       =                           0.0f,
    },


    [DRV_CURRENTSENSE_CHANNEL_BMS5_BMS6] =    // channel 11 = drv_tps2hb16ab.c
    {
    .adc_channel  = DRV_INPUTAD_ANALOG_MUX_LP9_SNS,
    .amp_per_volt = AMP_PER_VOLT_TPS2HB16AB,
    .offset       =                           0.0f,
    },
};