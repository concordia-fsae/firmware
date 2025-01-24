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
    HW_GPIO_I2C2_SCL = 0U,
    HW_GPIO_I2C2_SDA,
    HW_GPIO_SPI1_CLK,
    HW_GPIO_SPI1_MISO,
    HW_GPIO_SPI1_MOSI,
    HW_GPIO_SPI1_MAX_NCS,
    HW_GPIO_MUX_SEL1,
    HW_GPIO_MUX_SEL2,
    HW_GPIO_MUX_SEL3,
    HW_GPIO_CAN_RX,
    HW_GPIO_CAN_TX,
    HW_GPIO_TACH_FAN1,
    HW_GPIO_TACH_FAN2,
    HW_GPIO_LED,
    HW_GPIO_PWM_FAN1,
    HW_GPIO_PWM_FAN2,
    HW_GPIO_MAX_SAMPLE,
    HW_GPIO_NX3_NEN,
    HW_GPIO_ADC_MAX,
    HW_GPIO_ADC_MUX1,
    HW_GPIO_ADC_MUX2,
    HW_GPIO_ADC_MUX3,
    HW_GPIO_ADC_TEMPBRD1,
    HW_GPIO_ADC_TEMPBRD2,
    HW_GPIO_COUNT,
} HW_GPIO_pinmux_E;
