/**
 * @file drv_inputAD.h
 * @brief  Header file for the digital and analog input driver
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_inputAD_componentSpecific.h"
#include "HW_gpio.h"
#include "LIB_Types.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_INPUTAD_LOGIC_LOW = 0U,
    DRV_INPUTAD_LOGIC_HIGH,
} drv_inputAD_logicLevel_E;

typedef struct
{
    HW_GPIO_pinmux_E pin;
} drv_inputAD_configDigital_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t                drv_inputAD_getAnalogVoltage(drv_inputAD_channelAnalog_E channel);
drv_inputAD_logicLevel_E drv_inputAD_getLogicLevel(drv_inputAD_channelDigital_E channel);
