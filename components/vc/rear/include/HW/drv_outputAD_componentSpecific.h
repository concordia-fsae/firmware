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
    DRV_OUTPUTAD_DIGITAL_5V_EN1 = 0x00U,
    DRV_OUTPUTAD_DIGITAL_5V_EN2,
    DRV_OUTPUTAD_DIGITAL_MUX_SEL1,
    DRV_OUTPUTAD_DIGITAL_MUX_SEL2,
    DRV_OUTPUTAD_DIGITAL_HORN_EN,
    DRV_OUTPUTAD_DIGITAL_BMS_LIGHT_EN,
    DRV_OUTPUTAD_DIGITAL_IMD_LIGHT_EN,
    DRV_OUTPUTAD_DIGITAL_TSSI_G_EN,
    DRV_OUTPUTAD_DIGITAL_TSSI_R_EN,
    DRV_OUTPUTAD_DIGITAL_BR_LIGHT_EN,
    DRV_OUTPUTAD_DIGITAL_MCU_UART_EN,
    DRV_OUTPUTAD_DIGITAL_CS_IMU,
    DRV_OUTPUTAD_DIGITAL_CS_SD,
    DRV_OUTPUTAD_DIGITAL_LED,
    DRV_OUTPUTAD_DIGITAL_COUNT,
} drv_outputAD_channelDigital_E;
