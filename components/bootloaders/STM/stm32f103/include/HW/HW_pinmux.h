/*
 * HW_pinmux.h
 * PCB-specific GPIO configuration file. Will probably be autogenerated
 * eventually based on build config ID
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW.h"


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const GPIO_config_S pinmux [] = {
    // CAN pin settings
    { .port = GPIOA,       .pin = CAN_TX_PIN, .alternate_function = true,  .mode = GPIO_MODE_OUTPUT_HIGH_SPEED, .config = GPIO_CFG_OUTPUT_PUSH_PULL,},
    { .port = GPIOA,       .pin = CAN_RX_PIN, .alternate_function = false, .mode = GPIO_MODE_INPUT,             .config = GPIO_CFG_INPUT_FLOATING,},
    // other pin settings
    { .port = BUTTON_PORT, .pin = BUTTON_PIN, .alternate_function = false, .mode = GPIO_MODE_INPUT,             .config = GPIO_CFG_INPUT_FLOATING,},
    { .port = LED_PORT,    .pin = LED_PIN,    .alternate_function = false, .mode = GPIO_MODE_OUTPUT_LOW_SPEED,  .config = GPIO_CFG_OUTPUT_PUSH_PULL,},
};