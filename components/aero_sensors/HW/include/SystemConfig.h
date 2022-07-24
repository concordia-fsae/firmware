/**
 * @file SystemConfig.h
 * @Synopsis  Configuration of System pin{in,out}, system includes
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-02
 */

#pragma once


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stm32f1xx.h"

#include "ErrorHandler.h"
#include "FreeRTOSConfig.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

// System altering defines
#define USE_FULL_LL_DRIVER
#define ARS_RTOS_BLINK

// Interrupt priorities, tick interrupt is highest (lowest numerical value)
#define DMA_IRQ_PRIO    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 4U

// Pin aliases

// Com Bus

#define I2C1_SCL_Port GPIOB
#define I2C1_SCL_Pin GPIO_PIN_6
#define I2C1_SDA_Port GPIOB
#define I2C1_SDA_Pin GPIO_PIN_7
#define I2C2_SCL_Port GPIOB
#define I2C2_SCL_Pin GPIO_PIN_10
#define I2C2_SDA_Port GPIOB
#define I2C2_SDA_Pin GPIO_PIN_11

#define SD_NSS2_Port GPIOB
#define SD_NSS2_Pin GPIO_PIN_12
#define SD_SCK2_Port GPIOB
#define SD_SCK2_Pin GPIO_PIN_13
#define SD_MISO2_Port GPIOB
#define SD_MISO2_Pin GPIO_PIN_14
#define SD_MOSI2_Port GPIOB
#define SD_MOSI2_Pin GPIO_PIN_15

// Input pins
// Analog Signals

// Digital Signals
#define BUTTON_Port GPIOA
#define BUTTON_Pin GPIO_PIN_7

// Output pins
// Analog Signals

// Digital Signals
#define LED_R_Port GPIOA
#define LED_R_Pin GPIO_PIN_4
#define LED_G_Port GPIOA
#define LED_G_Pin GPIO_PIN_5
#define LED_B_Port GPIOA
#define LED_B_Pin GPIO_PIN_6

#define LED_GPIO_Port GPIOC
#define LED_Pin GPIO_PIN_13

// TODO: Class each pin to respective category/type and use
#define CAN_TX_Port GPIOB
#define CAN_TX_Pin GPIO_PIN9
#define CAN_RX_Port GPIOB
#define CAN_RX_Pin GPIO_PIN_8

