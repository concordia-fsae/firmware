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
#include "stdbool.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#if defined (BMSW_BOARD_VA3) & defined (BMSW_BOARD_VA3)
_Static_assert(true, "Cannot have both VA1 and VA3 defined simultaneously");
#endif

// system altering defines
#define USE_FULL_LL_DRIVER

// Interrupt priorities, lower number is higher priority
// tick interrupt is highest priority
#define DMA_IRQ_PRIO       configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 4U
#define ADC_IRQ_PRIO       DMA_IRQ_PRIO + 1U
#define CAN_RX_IRQ_PRIO    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 6U
#define CAN_TX_IRQ_PRIO    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 8U

/**< Pin Aliases */
#if defined (BMSW_BOARD_VA1)
// communication busses
#define I2C2_SCL_Pin   GPIO_PIN_10
#define I2C2_SDA_Pin   GPIO_PIN_11
#define I2C2_GPIO_Port GPIOB

#define SPI1_CLK_Pin      GPIO_PIN_3
#define SPI1_MISO_Pin     GPIO_PIN_4
#define SPI1_MOSI_Pin     GPIO_PIN_5
#define SPI1_GPIO_Port    GPIOB
#define SPI1_MAX_NCS_Pin  GPIO_PIN_12
#define SPI1_MAX_NCS_Port GPIOB
#define SPI1_LTC_NCS_Pin  GPIO_PIN_13
#define SPI1_LTC_NCS_Port GPIOB


// input pins
#define A0_Pin       GPIO_PIN_0
#define A0_GPIO_Port GPIOB

#define A1_Pin       GPIO_PIN_7
#define A1_GPIO_Port GPIOA

#define A2_Pin       GPIO_PIN_6
#define A2_GPIO_Port GPIOA

#define ADC_CHANNEL_CELL_MEASUREMENT ADC_CHANNEL_0
#define CELL_VOLTAGE_Pin             GPIO_PIN_0
#define CELL_VOLTAGE_Port            GPIOA

#define LTC_INTERRUPT_Pin  GPIO_PIN_15
#define LTC_INTERRUPT_Port GPIOB
#define LTC_NRST_Pin       GPIO_PIN_14
#define LTC_NRST_Port      GPIOB

// output pins
#define LED_Pin       GPIO_PIN_13
#define LED_GPIO_Port GPIOC

#define FAN_Pin       GPIO_PIN_8
#define FAN_GPIO_Port GPIOA

#define MAX_SAMPLE_Pin       GPIO_PIN_10
#define MAX_SAMPLE_GPIO_Port GPIOA

/**< BMSW_BOARD_VA1 */
#elif defined (BMSW_BOARD_VA3)
/**< Pin Aliases */
/**< Communication Busses */
#define I2C2_SCL_Pin   GPIO_PIN_10
#define I2C2_SDA_Pin   GPIO_PIN_11
#define I2C2_GPIO_Port GPIOB

#define SPI1_CLK_Pin      GPIO_PIN_3
#define SPI1_MISO_Pin     GPIO_PIN_4
#define SPI1_MOSI_Pin     GPIO_PIN_5
#define SPI1_GPIO_Port    GPIOB
#define SPI1_MAX_NCS_Pin  GPIO_PIN_12
#define SPI1_MAX_NCS_Port GPIOB

#define MUX_SEL1_Pin  GPIO_PIN_15
#define MUX_SEL1_Port GPIOB
#define MUX_SEL2_Pin  GPIO_PIN_14
#define MUX_SEL2_Port GPIOB
#define MUX_SEL3_Pin  GPIO_PIN_13
#define MUX_SEL3_Port GPIOB

#define CAN_RXD_Pin GPIO_PIN_8
#define CAN_TXD_Pin GPIO_PIN_9
#define CAN_Port    GPIOB

/**< Input Pins */
#define A1_Pin       GPIO_PIN_0
#define A1_GPIO_Port GPIOB

#define A2_Pin       GPIO_PIN_6
#define A2_GPIO_Port GPIOA

#define A3_Pin       GPIO_PIN_7
#define A3_GPIO_Port GPIOA

#define FAN1_SPEED_Pin       GPIO_PIN_8
#define FAN1_SPEED_GPIO_Port GPIOA
#define FAN2_SPEED_Pin       GPIO_PIN_9
#define FAN2_SPEED_GPIO_Port GPIOA

/**< Output Pins */
#define LED_Pin       GPIO_PIN_13
#define LED_GPIO_Port GPIOC

#define FAN1_PWM_Pin       GPIO_PIN_6
#define FAN1_PWM_GPIO_Port GPIOB
#define FAN2_PWM_Pin       GPIO_PIN_7
#define FAN2_PWM_GPIO_Port GPIOB

#define MAX_SAMPLE_Pin       GPIO_PIN_10
#define MAX_SAMPLE_GPIO_Port GPIOA

#define NX3_NEN_Pin  GPIO_PIN_15
#define NX3_NEN_Port GPIOA

/**< Analog Pins */
#define ADC_CHANNEL_CELL_MEASUREMENT ADC_CHANNEL_0
#define CELL_VOLTAGE_Pin             GPIO_PIN_0
#define CELL_VOLTAGE_Port            GPIOA

#define ADC_CHANNEL_MUX1 ADC_CHANNEL_1
#define MUX1_Pin         GPIO_PIN_1
#define MUX1_Port        GPIOA
#define ADC_CHANNEL_MUX2 ADC_CHANNEL_2
#define MUX2_Pin         GPIO_PIN_2
#define MUX2_Port        GPIOA
#define ADC_CHANNEL_MUX3 ADC_CHANNEL_3
#define MUX3_Pin         GPIO_PIN_3
#define MUX3_Port        GPIOA
#define ADC_CHANNEL_BRD1 ADC_CHANNEL_4
#define BRD1_Pin         GPIO_PIN_4
#define BRD1_Port        GPIOA
#define ADC_CHANNEL_BRD2 ADC_CHANNEL_5
#define BRD2_Pin         GPIO_PIN_5
#define BRD2_Port        GPIOA

#endif /**< BMSW_BOARD_VA3 */
