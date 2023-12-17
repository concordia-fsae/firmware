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
#define SW5_Pin          GPIO_PIN_9
#define SW5_GPIO_Port    GPIOB


// output pins
#define LED_Pin               GPIO_PIN_13
#define LED_GPIO_Port         GPIOC

