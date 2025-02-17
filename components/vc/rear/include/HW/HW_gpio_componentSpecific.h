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
} HW_GPIO_pinmux_E;
