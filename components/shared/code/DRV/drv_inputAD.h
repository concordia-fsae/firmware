/**
 * @file drv_inputAD.h
 * @brief  Header file for the digital and analog input driver
 *
 * Setup
 * 1. Define the digital and analog channels in drv_inputAD_componentSpecific.h
 *    and name them drv_inputAD_channelAnalog_E and drv_inputAD_getLogicLevel.
 * 2. Configure the digital channels in drv_inputAD_componentSpecific.c and name
 *    them drv_inputAD_configDigital
 * 3. Include drv_inputAD_private.h in drv_inputAD_componentSpecific.c and call
 *    the drv_inputAD_private_init function
 * 4. Periodically call the drv_inputAD_private_runDigital function to update
 *    the digital inputs and load new voltages with drv_inputAD_setAnalogVoltage
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_io.h"
#include "drv_inputAD_componentSpecific.h"
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
        INPUT_DIGITAL,
        INPUT_DIGITAL_CAN,
    } type;
    const union
    {
        drv_io_pinConfig_S     gpio;
        CANRX_MESSAGE_health_E (*canrx_digitalStatus)(CAN_digitalStatus_E*);
    } config;
} drv_inputAD_configDigital_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t            drv_inputAD_getAnalogVoltage(drv_inputAD_channelAnalog_E channel);
drv_io_logicLevel_E  drv_inputAD_getLogicLevel(drv_inputAD_channelDigital_E channel);
drv_io_activeState_E drv_inputAD_getDigitalActiveState(drv_inputAD_channelDigital_E channel);
