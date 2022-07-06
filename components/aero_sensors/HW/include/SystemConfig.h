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

// Pin aliases

// Input pins
// Analog Signals

#define AMUX_1_Port GPIOA
#define AMUX_1_Pin GPIO_PIN_0
#define AMUX_2_Port GPIOA
#define AMUX_2_Pin GPIO_PIN_1
#define AMUX_3_Port GPIOA
#define AMUX_3_Pin GPIO_PIN_2
#define AMUX_4_Port GPIOA
#define AMUX_4_Pin GPIO_PIN_3

// Digital Signals


// Output pins
// Analog Signals

// Digital Signals

#define LED_GPIO_Port GPIOC
#define LED_Pin GPIO_PIN_13

#define AMUX_S0_Port GPIOA
#define AMUX_S0_Pin GPIO_PIN_8
#define AMUX_S1_Port GPIOA
#define AMUX_S1_Pin GPIO_PIN_9

// TODO: Class each pin to respective category/type and use
#define CAN_TX_Port GPIOB
#define CAN_TX_Pin GPIO_PIN9
#define CAN_RX_Port GPIOB
#define CAN_RX_Pin GPIO_PIN_8

#define I2C1_SCL_Port GPIOB
#define I2C1_SCL_Pin GPIO_PIN_6
#define I2C1_SDA_Port GPIOB
#define I2C1_SDA_Pin GPIO_PIN_7

#define SD_NSS2_Port GPIOB
#define SD_NSS2_Pin GPIO_PIN_12
#define SD_SCK2_Port GPIOB
#define SD_SCK2_Pin GPIO_PIN_13
#define SD_MISO2_Port GPIOB
#define SD_MISO2_Pin GPIO_PIN_14
#define SD_MOSI2_Port GPIOB
#define SD_MOSI2_Pin GPIO_PIN_15

