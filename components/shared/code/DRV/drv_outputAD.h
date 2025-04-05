/**
 * @file drv_outputAD.h
 * @brief  Header file for the digital and analog input driver
 *
 * Setup
 * 1. Define the digital and analog channels in drv_outputAD_componentSpecific.h
 *    and name them drv_outputAD_channelAnalog_E and drv_outputAD_channelDigital_E.
 * 2. Configure the digital channels in drv_outputAD_componentSpecific.c and name
 *    them drv_outputAD_configDigital
 * 3. Include drv_outputAD_private.h in drv_outputAD_componentSpecific.c and call
 *    the drv_outputAD_private_init function
 *
 * Usage
 * - To set a pin state, use that drv_outputAD_setDigitalActiveState or
 *   drv_outputAD_setAnalogVoltage functions.
 * - To see the current state of a pin, use the drv_outputAD_getDigitalActiveState
 *   and drv_outputAD_getAnalogVoltage functions.
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_io.h"
#include "drv_outputAD_componentSpecific.h"
#include "HW_gpio.h"
#include "LIB_Types.h"
#include "CANTypes_generated.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    const enum
    {
        OUTPUT_DIGITAL,
    } type;
    const union
    {
        drv_io_pinConfig_S     gpio;
    } config;
} drv_outputAD_configDigital_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void                 drv_outputAD_setAnalogVoltage(drv_outputAD_channelAnalog_E channel, float32_t voltage);
void                 drv_outputAD_setDigitalActiveState(drv_outputAD_channelDigital_E channel, drv_io_activeState_E state);
void                 drv_outputAD_toggleDigitalState(drv_outputAD_channelDigital_E channel);
float32_t            drv_outputAD_getAnalogVoltage(drv_outputAD_channelAnalog_E channel);
drv_io_activeState_E drv_outputAD_getDigitalActiveState(drv_outputAD_channelDigital_E channel);
