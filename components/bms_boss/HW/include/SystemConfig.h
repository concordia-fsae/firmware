/**
 * @file SystemConfig.h
 * @brief  Define System Configuration
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "ErrorHandler.h"
#include "stdbool.h"

// Firmware Includes
#include "stm32f1xx.h"

// FreeRTOS Includes
#include "FreeRTOSConfig.h"

// Other Includes
#include "FloatTypes.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

// System altering defines
#define USE_FULL_LL_DRIVER

// NVIC interrupt priorities, lower number is higher priority
// tick interrupt is highest priority
#define DMA_IRQ_PRIO    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 4U
#define ADC_IRQ_PRIO    DMA_IRQ_PRIO + 1U
#define CAN_RX_IRQ_PRIO configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 6U
#define CAN_TX_IRQ_PRIO configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 8U
#define EXTI_IRQ_PRIO   ADC_IRQ_PRIO

// Pin Aliases
// Communications
#define CAN_R_Pin GPIO_PIN_8
#define CAN_T_Pin GPIO_PIN_9
#define CAN_Port  GPIOB

#define I2C_SCL_Pin GPIO_PIN_6
#define I2C_SDA_Pin GPIO_PIN_7
#define I2C_Port    GPIOB

// Data
#define M_HLS1_Pin  GPIO_PIN_9
#define M_HLS1_Port GPIOA

#define M_HLS2_Pin  GPIO_PIN_4
#define M_HLS2_Port GPIOB

// Input
#define TSMS_CHG_Pin  GPIO_PIN_14
#define TSMS_CHG_Port GPIOB

#define OK_HS_Pin  GPIO_PIN_8
#define OK_HS_Port GPIOA

#define BMS_IMD_Reset_Pin  GPIO_PIN_2
#define BMS_IMD_Reset_Port GPIOA

#define IMD_STATUS_MEM_Pin  GPIO_PIN_4
#define IMD_STATUS_MEM_Port GPIOA

#define BMS_STATUS_MEM_Pin  GPIO_PIN_5
#define BMS_STATUS_MEM_Port GPIOA

#define ADC_N_CHANNEL ADC_CHANNEL_0
#define ADC_N_Pin     GPIO_PIN_0
#define ADC_N_Port    GPIOB

#define ADC_P_CHANNEL ADC_CHANNEL_1
#define ADC_P_Pin     GPIO_PIN_1
#define ADC_P_Port    GPIOB

// Output
#define BMS_STATUS_Pin  GPIO_PIN_12
#define BMS_STATUS_Port GPIOB

#define IMD_STATUS_Pin  GPIO_PIN_13
#define IMD_STATUS_Port GPIOB

#define AIR_Pin  GPIO_PIN_0
#define AIR_Port GPIOA

#define PCHG_Pin  GPIO_PIN_1
#define PCHG_Port GPIOA

#define LED_Pin  GPIO_PIN_13
#define LED_Port GPIOC
