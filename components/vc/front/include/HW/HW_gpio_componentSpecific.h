/**
 * @file HW_gpio_componentSpecific.h
 * @brief  Header file for GPIO firmware component specific
 */

#pragma once

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    HW_GPIO_CAN1_RX = 0U,
    HW_GPIO_CAN1_TX,
    HW_GPIO_CAN2_RX,
    HW_GPIO_CAN2_TX,
    HW_GPIO_LED,
    HW_GPIO_COUNT,
    HW_GPIO_DIG_SPARE_1,
    HW_GPIO_DIG_SPARE_2,
    HW_GPIO_DIG_SPARE3,
    HW_GPIO_DIG_SPARE4,
    HW_GPIO_LED,
    HW_GPIO_AIN_SPARE1,
    HW_GPIO_AIN_SPARE2,
    HW_GPIO_AIN_SPARE3,
    HW_GPIO_AIN_SPARE4,
    HW_GPIO_APPS_P1,
    HW_GPIO_APPS_P2,
    HW_GPIO_BR_POT,
    HW_GPIO_L_SHK_DISP,
    HW_GPIO_L_BR_TMP,
    HW_GPIO_R_SHK_DISP,
    HW_GPIO_R_BR_TMP,
    HW_GPIO_AIN_PU1,
    HW_GPIO_AIN_PU2,
    HW_GPIO_BR_PR,
    HW_GPIO_TEMP_1,
    HW_GPIO_5V_FLT2,
    HW_GPIO_5V_EN2,
    HW_GPIO_5V_EN1,
    HW_GPIO_5V_FLT1,
    HW_GPIO_RXD2,
    HW_GPIO_TXD2,
    HW_GPIO_SEL2,
    HW_GPIO_SEL1,
    HW_GPIO_SG_EN,
    HW_GPIO_HORN_EN,
    HW_GPIO_BMS_LIGHT_EN,
    HW_GPIO_IMD_LIGHT_EN,
    HW_GPIO_TSSI_G_EN,
    HW_GPIO_TSSI_R_EN,
    HW_GPIO_BR_LIGHT_EN,
    HW_GPIO_RXD1,
    HW_GPIO_TXD1,
    HW_GPIO_SWDIO,
    HW_GPIO_SWCLK,
    HW_GPIO_UART_EN,
    HW_GPIO_MCU_UART_TX,
    HW_GPIO_MCU_UART_RX,
    HW_GPIO_CS_IMU,
    HW_GPIO_CS_SD,
    HW_GPIO_SPI_SCK,
    HW_GPIO_SPI_MISO,
    HW_GPIO_SPI_MOSI,
    HW_GPIO_FLOW,
    HW_GPIO_R_WS,
    HW_GPIO_L_WS,
    HW_GPIO_RUN,
} HW_GPIO_pinmux_E;
