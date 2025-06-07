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
    HW_GPIO_LED = 0U,
    HW_GPIO_CAN1_RX,
    HW_GPIO_CAN1_TX,
    HW_GPIO_CAN_SLEEP,
    HW_GPIO_DIN1,
    HW_GPIO_DIN2,
    HW_GPIO_DIN3,
    HW_GPIO_DIN4,
    HW_GPIO_DIN5,
    HW_GPIO_DIN6,
    HW_GPIO_DIN7,
    HW_GPIO_DIN8,
    HW_GPIO_DIN9,
    HW_GPIO_DIN10,
    HW_GPIO_AIN1,
    HW_GPIO_AIN2,
    HW_GPIO_AIN3,
    HW_GPIO_AIN4,
    HW_GPIO_AIN5,
    HW_GPIO_AIN6,
    HW_GPIO_AIN7,
    HW_GPIO_COUNT,
} HW_GPIO_pinmux_E;
