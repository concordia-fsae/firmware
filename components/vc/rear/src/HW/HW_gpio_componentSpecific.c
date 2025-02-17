/**
 * @file HW_gpio_componentSpecific.c
 * @brief  Source code for GPIO firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW_gpio.h"

const HW_GPIO_S HW_GPIO_pinmux[HW_GPIO_COUNT] = {
    [HW_GPIO_CAN1_RX] = {
        .port = GPIOB,
        .pin = GPIO_PIN_8,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_CAN1_TX] = {
        .port = GPIOB,
        .pin = GPIO_PIN_9,
        .mode = GPIO_MODE_AF_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINSET,
    },
    [HW_GPIO_CAN2_RX] = {
        .port = GPIOB,
        .pin = GPIO_PIN_12,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_CAN2_TX] = {
        .port = GPIOB,
        .pin = GPIO_PIN_13,
        .mode = GPIO_MODE_AF_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINSET,
    },
    [HW_GPIO_LED] = {
        .port = GPIOC,
        .pin = GPIO_PIN_13,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
};
