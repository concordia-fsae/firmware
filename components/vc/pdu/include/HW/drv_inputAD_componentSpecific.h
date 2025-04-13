/**
 * @file IO.h
 * @brief  Header file for IO Module
 */

#pragma once

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_INPUTAD_TSCHG_MS = 0x00,
    DRV_INPUTAD_RUN_BUTTON,
    DRV_INPUTAD_DIGITAL_COUNT,
} drv_inputAD_channelDigital_E;

typedef enum
{
    DRV_INPUTAD_ANALOG_COUNT,
} drv_inputAD_channelAnalog_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void drv_inputAD_init_componentSpecific(void);
void drv_inputAD_1kHz_componentSpecific(void);
