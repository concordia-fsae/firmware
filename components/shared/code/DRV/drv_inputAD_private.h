/**
 * @file drv_inputAD_private.h
 * @brief  Private header file for the digital and analog input driver internal interface
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_inputAD.h"

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void drv_inputAD_private_init(void);
void drv_inputAD_private_runDigital(void);
void drv_inputAD_private_setAnalogVoltage(drv_inputAD_channelAnalog_E channel, float32_t voltage);
