/**
 * @file drv_currentSense.h
 * @brief Header file for the current sensing for VN9008, and high side driver
 *
 * Setup
 *
 * Usage
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_currentSense_componentSpecific.h"
#include "drv_inputAD.h"
#include "LIB_Types.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    const drv_inputAD_channelAnalog_E adc_channel;  // Channel passed to AD driver to read voltage
    const float32_t                   amp_per_volt; // Ratio I/V
    const float32_t                   offset;       // Voltage offset
}drv_currentSense_S;

// Declare the array defined in componentSpecific.c so it can be accessed here. Cannot access componentSpecifc.c if we only include componentSpecifc.h
extern drv_currentSense_S
    drv_currentSense_spec[DRV_CURRENTSENSE_CHANNEL_COUNT];

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

// f(channel) = current = [voltage-offset] * amp/volt = [g(adc_channel)-offset] * amp/volt
float32_t drv_currentSense_getCurrent(
    drv_currentSense_channel_E const channel
    );