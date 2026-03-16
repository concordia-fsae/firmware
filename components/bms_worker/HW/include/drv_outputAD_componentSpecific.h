/**
 * @file drv_outputAD_componentSpecific.h
 * @brief Header file for the component specific output driver
 */

#pragma once

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_OUTPUTAD_ANALOG_COUNT,
} drv_outputAD_channelAnalog_E;

typedef enum
{
    DRV_OUTPUTAD_DIGITAL_MUX_SEL1 = 0x00U,
    DRV_OUTPUTAD_DIGITAL_MUX_SEL2,
    DRV_OUTPUTAD_DIGITAL_MUX_SEL3,
    DRV_OUTPUTAD_DIGITAL_COUNT,
} drv_outputAD_channelDigital_E;
