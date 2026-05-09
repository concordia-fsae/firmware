/**
 * @file drv_currentSense_componentSpecific.c
 * @brief  Component specific source file for current sensing driver on the BMS pack. (only 1 channel : Pack)
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_currentSense.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

drv_currentSense_S drv_currentSense_spec[DRV_CURRENTSENSE_CHANNEL_COUNT] =
{
    [DRV_CURRENTSENSE_CHANNEL_PACK] =    // channel 0
    {
    .adc_channel  = DRV_INPUTAD_ANALOG_CS,
    .amp_per_volt =               -400.0f,
    .offset       =                  0.0f,
    },
};