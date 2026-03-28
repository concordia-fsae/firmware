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
#if !((APP_VARIANT_ID == 1U) && ((BMSW_NODE_ID % 2) == 0U))
    HW_GPIO_TACH_FAN1,
    HW_GPIO_TACH_FAN2,
#endif
    HW_GPIO_LED,
#if !((APP_VARIANT_ID == 1U) && ((BMSW_NODE_ID % 2) == 0U))
    HW_GPIO_PWM_FAN1,
    HW_GPIO_PWM_FAN2,
#endif
#if (APP_VARIANT_ID == 0U)
    HW_GPIO_MAX_SAMPLE,
    HW_GPIO_NX3_NEN,
#endif
    HW_GPIO_ADC_MAX,
    HW_GPIO_ADC_MUX1,
#if (APP_VARIANT_ID == 0U)
    HW_GPIO_ADC_MUX2,
    HW_GPIO_ADC_MUX3,
#endif
    HW_GPIO_ADC_TEMP_BALANCING1,
    HW_GPIO_ADC_TEMP_BALANCING2,
#if (APP_VARIANT_ID == 1U)
    HW_GPIO_ADC_VSNS_7V5,
    HW_GPIO_THERM9,
    HW_GPIO_ADC_TEMP_BOARD,
    HW_GPIO_NSHUTDOWN,
    HW_GPIO_SUPPLY_KEEPON,
#endif
    HW_GPIO_COUNT,
} HW_GPIO_pinmux_E;
