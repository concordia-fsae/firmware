/**
 * @file SystemConfig.h
 * @Synopsis  Configuration of System pin{in,out}, system includes
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-02
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stm32f1xx.h"

#include "ErrorHandler.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

// System altering defines
#define USE_FULL_LL_DRIVER

// Interrupt priorities, tick interrupt is highest (lowest numerical value)
#define DMA_IRQ_PRIO    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 4U
#define CAN_IRQ_PRIO    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 6U

// Pin aliases

// Input pins
// Analog Signals

// Digital Signals


// Output pins
// Analog Signals

// Digital Signals
#define LED_GPIO_Port GPIOC
#define LED_Pin GPIO_PIN_13
