/**
 * @file DRV_inputAD.h
 * @brief  Header file for the digital and analog input driver
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "DRV_inputAD_componentSpecific.h"
#include "LIB_Types.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_INPUTAD_LOGIC_LOW = 0U,
    DRV_INPUTAD_LOGIC_HIGH,
} DRV_inputAD_logicLevel_E;

typedef struct
{
    HW_GPIO_pinmux_E pin;
} DRV_inputAD_configDigital_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void                     DRV_inputAD_init(void);
float32_t                DRV_inputAD_getAnalogVoltage(DRV_inputAD_channelAnalog_E channel);
DRV_inputAD_logicLevel_E DRV_inputAD_getLogicLevel(DRV_inputAD_channelDigital_E channel);
