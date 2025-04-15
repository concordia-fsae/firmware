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
    HW_GPIO_I2C1_SCL,
    HW_GPIO_I2C1_SDA,
    HW_GPIO_MHLS,
    HW_GPIO_TSMS_CHG,
    HW_GPIO_OK_HS,
    HW_GPIO_BMS_IMD_RESET,
    HW_GPIO_IMD_STATUS_MEM,
    HW_GPIO_BMS_STATUS_MEM,
    HW_GPIO_ADC_N,
    HW_GPIO_ADC_P,
    HW_GPIO_BMS_STATUS,
#if BMSB_CONFIG_ID == 0U
    HW_GPIO_IMD_STATUS,
#endif
    HW_GPIO_AIR,
    HW_GPIO_PCHG,
    HW_GPIO_LED,
#if BMSB_CONFIG_ID == 1U
    HW_GPIO_UART_TX_3V,
    HW_GPIO_UART_RX_3V,
    HW_GPIO_ADC_TEMP,
    HW_GPIO_VPACK_ADC_P,
    HW_GPIO_VPACK_ADC_N,
    HW_GPIO_VPACK_DIAG,
    HW_GPIO_CAN2_RX,
    HW_GPIO_CAN2_TX,
#endif
    HW_GPIO_COUNT,
} HW_GPIO_pinmux_E;
