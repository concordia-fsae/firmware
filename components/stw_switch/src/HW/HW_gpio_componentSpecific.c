/**
 * @file HW_gpio_componentSpecific.c
 * @brief  Source code for GPIO firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW_gpio.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

const HW_GPIO_S HW_GPIO_pinmux[HW_GPIO_COUNT] = {
    [HW_GPIO_LED] = {
        .port = GPIOC,
        .pin = GPIO_PIN_13,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_CAN1_RX] = {
        .port = GPIOA,
        .pin = GPIO_PIN_11,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_CAN1_TX] = {
        .port = GPIOA,
        .pin = GPIO_PIN_12,
        .mode = GPIO_MODE_AF_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_CAN_SLEEP] = {
        .port = GPIOA,
        .pin = GPIO_PIN_8,
        .mode = GPIO_MODE_OUTPUT_OD,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_DIN1] = {
        .port = GPIOB,
        .pin = GPIO_PIN_5,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_DIN2] = {
        .port = GPIOB,
        .pin = GPIO_PIN_7,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_DIN3] = {
        .port = GPIOB,
        .pin = GPIO_PIN_8,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_DIN4] = {
        .port = GPIOB,
        .pin = GPIO_PIN_9,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_DIN5] = {
        .port = GPIOB,
        .pin = GPIO_PIN_10,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_DIN6] = {
        .port = GPIOB,
        .pin = GPIO_PIN_11,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_DIN7] = {
        .port = GPIOB,
        .pin = GPIO_PIN_12,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_DIN8] = {
        .port = GPIOB,
        .pin = GPIO_PIN_13,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_DIN9] = {
        .port = GPIOB,
        .pin = GPIO_PIN_14,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_DIN10] = {
        .port = GPIOB,
        .pin = GPIO_PIN_15,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_AIN1] = {
        .port = GPIOA,
        .pin = GPIO_PIN_1,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_AIN2] = {
        .port = GPIOA,
        .pin = GPIO_PIN_2,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_AIN3] = {
        .port = GPIOA,
        .pin = GPIO_PIN_3,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_AIN4] = {
        .port = GPIOA,
        .pin = GPIO_PIN_4,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_AIN5] = {
        .port = GPIOA,
        .pin = GPIO_PIN_5,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_AIN6] = {
        .port = GPIOA,
        .pin = GPIO_PIN_6,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_AIN7] = {
        .port = GPIOA,
        .pin = GPIO_PIN_7,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
};
