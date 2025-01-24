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
    [HW_GPIO_I2C1_SCL] = {
        .port = GPIOB,
        .pin = GPIO_PIN_6,
        .mode = GPIO_MODE_AF_OD,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_I2C1_SDA] = {
        .port = GPIOB,
        .pin = GPIO_PIN_7,
        .mode = GPIO_MODE_AF_OD,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_MHLS] = {
        .port = GPIOA,
        .pin = GPIO_PIN_9,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_TSMS_CHG] = {
        .port = GPIOB,
        .pin = GPIO_PIN_14,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_OK_HS] = {
        .port = GPIOA,
        .pin = GPIO_PIN_8,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_BMS_IMD_RESET] = {
        .port = GPIOA,
        .pin = GPIO_PIN_2,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_IMD_STATUS_MEM] = {
        .port = GPIOA,
        .pin = GPIO_PIN_4,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_BMS_STATUS_MEM] = {
        .port = GPIOA,
        .pin = GPIO_PIN_5,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_N] = {
        .port = GPIOB,
        .pin = GPIO_PIN_0,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_P] = {
        .port = GPIOB,
        .pin = GPIO_PIN_1,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_BMS_STATUS] = {
        .port = GPIOB,
        .pin = GPIO_PIN_12,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_PULLDOWN,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_IMD_STATUS] = {
        .port = GPIOB,
        .pin = GPIO_PIN_13,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_PULLDOWN,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_AIR] = {
        .port = GPIOA,
        .pin = GPIO_PIN_0,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_PCHG] = {
        .port = GPIOA,
        .pin = GPIO_PIN_1,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
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
