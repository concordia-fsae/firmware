/*
 * systemConfig.h
 * Define system configuration
 */

#pragma once

#include "stm32f1xx.h"
#include "ErrorHandler.h"
#include "FloatTypes.h"

// Defines

// system altering defines
#define USE_FULL_LL_DRIVER

// Interrupt priorities, lower number is higher priority
// tick interrupt is highest priority

#define DMA_IRQ_PRIO TICK_INT_PRIORITY// + 1U
#define CAN_IRQ_PRIO TICK_INT_PRIORITY// + 2U

// pin aliases
#define LED_Pin                  GPIO_PIN_13
#define LED_GPIO_Port            GPIOC

#define CURR_SENSE_Pin           GPIO_PIN_0
#define CURR_SENSE_GPIO_Port     GPIOA

#define TEMP_BRD_Pin             GPIO_PIN_1
#define TEMP_BRD_GPIO_Port       GPIOA

#define TEMP_GPU_Pin             GPIO_PIN_2
#define TEMP_GPU_GPIO_Port       GPIOA

#define SPI1_NSS_Pin             GPIO_PIN_4
#define SPI1_NSS_GPIO_Port       GPIOA

#define PADDLE_LEFT_Pin          GPIO_PIN_0
#define PADDLE_LEFT_GPIO_Port    GPIOB

#define PADDLE_RIGHT_Pin         GPIO_PIN_1
#define PADDLE_RIGHT_GPIO_Port   GPIOB

#define BTN_MIDDLE_Pin           GPIO_PIN_2
#define BTN_MIDDLE_GPIO_Port     GPIOB

#define SL_DATA_Pin              GPIO_PIN_10
#define SL_DATA_GPIO_Port        GPIOB

#define ROT_ENC_1_A_Pin          GPIO_PIN_11
#define ROT_ENC_1_A_GPIO_Port    GPIOB
#define ROT_ENC_1_B_Pin          GPIO_PIN_12
#define ROT_ENC_1_B_GPIO_Port    GPIOB

#define ROT_ENC_2_A_Pin          GPIO_PIN_13
#define ROT_ENC_2_A_GPIO_Port    GPIOB
#define ROT_ENC_2_B_Pin          GPIO_PIN_14
#define ROT_ENC_2_B_GPIO_Port    GPIOB

#define FT_PDN_Pin               GPIO_PIN_3
#define FT_PDN_GPIO_Port         GPIOB

#define BTN_TOP_LEFT_Pin         GPIO_PIN_4
#define BTN_TOP_LEFT_GPIO_Port   GPIOB

#define BTN__TOP_RIGHT_Pin       GPIO_PIN_5
#define BTN__TOP_RIGHT_GPIO_Port GPIOB

#define SW1_Pin                  GPIO_PIN_6
#define SW1_GPIO_Port            GPIOB

#define SW2_Pin                  GPIO_PIN_7
#define SW2_GPIO_Port            GPIOB

#define SW3_Pin                  GPIO_PIN_8
#define SW3_GPIO_Port            GPIOB

#define SW4_Pin                  GPIO_PIN_9
#define SW4_GPIO_Port            GPIOB

