/**
 * systemConfig.h
 * Define system configuration
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stm32f1xx.h"

#include "ErrorHandler.h"
#include "FloatTypes.h"
#include "FreeRTOSConfig.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

// system altering defines
#define USE_FULL_LL_DRIVER

// Interrupt priorities, lower number is higher priority
// tick interrupt is highest priority
#define DMA_IRQ_PRIO       configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 4U
#define CAN_RX_IRQ_PRIO    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 6U
#define CAN_TX_IRQ_PRIO    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 8U

// pin aliases

// input pins
#define A0_Pin          GPIO_PIN_0
#define A0_GPIO_Port    GPIOB

#define A1_Pin          GPIO_PIN_7
#define A1_GPIO_Port    GPIOA

#define A2_Pin          GPIO_PIN_6
#define A2_GPIO_Port    GPIOA

// output pins
#define LED_Pin               GPIO_PIN_13
#define LED_GPIO_Port         GPIOC

#define FAN_Pin               GPIO_PIN_8
#define FAN_GPIO_Port         GPIOA
