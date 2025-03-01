/**
 * @file SystemConfig.h
 * @brief  Define System Configuration
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "stdbool.h"

// Firmware Includes
#include "stm32f1xx.h"

// FreeRTOS Includes
#include "FreeRTOSConfig.h"

// Other Includes
#include "FloatTypes.h"
#include "FeatureDefines_generated.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

// system altering defines
#define USE_FULL_LL_DRIVER

// Interrupt priorities, lower number is higher priority
// tick interrupt is highest priority
#define DMA_IRQ_PRIO    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 4U
#define ADC_IRQ_PRIO    DMA_IRQ_PRIO + 1U
#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
#define TIM_IRQ_PRIO    ADC_IRQ_PRIO + 1U
#endif // FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
#define CAN_RX_IRQ_PRIO configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 7U
#define CAN_TX_IRQ_PRIO configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 8U
