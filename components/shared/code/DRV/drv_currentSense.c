/**
 * @file drv_currentSense.c
 * @brief  Source file for generic current sensing function
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_currentSense.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t drv_currentSense_getCurrent(drv_currentSense_channel_E const channel)
{
    const drv_currentSense_S *channelIn = &drv_currentSense_spec[channel];

    float32_t                voltage    = drv_inputAD_getAnalogVoltage(channelIn->adc_channel); // Voltage defined with inputAD driver

    return((voltage - (channelIn->offset)) * channelIn->amp_per_volt);                          // I = (V-offset) * (I/V)
}