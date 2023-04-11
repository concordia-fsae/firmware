/**
 * @file SystemConfig.h
 * @brief  Header file for the system configuration
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2023-01-13
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stm32f1xx.h"

#include "ErrorHandler.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define BMS_STATUS_ID 0x0037
#define BMS_FLAG_BYTE 0x00
#define BMS_ISREADY_BIT  0x02
#define BMS_ISCHARGE_BIT 0x03
#define BMS_ISERROR_BIT 0x07
#define BMS_VOLTAGE_ID 0x32
#define BMS_BATT_VOLTAGE_MSB 0x01
#define BMS_BATT_VOLTAGE_LSB 0x00

#define MOTOR_CONTROLLER_BASE_ID 0x01
#define MOTOR_CONTROLLER_HIGHSPEED_MESSAGE_ID MOTOR_CONTROLLER_BASE_ID + 0x10
#define MC_DC_BUS_VOLTAGE_MSB 7
#define MC_DC_BUS_VOLTAGE_LSB 6

#define SAFETY_BOARD_STATUS_ID 0x40

#define MOTOR_CONTROLLER_TIMEOUT 100
#define BMS_TIMEOUT 100

#define EXTI_IRQ_PRIO TICK_INT_PRIORITY + 0x01
#define CAN_IRQ_PRIO  EXTI_IRQ_PRIO + 0x01
#define CYCLE_IRQ_PRIO CAN_IRQ_PRIO + 0x01
/**
 * Pin Description
 * B12 -> BMS Status [out] ** Digital
 * B13 -> IMD Status [out] ** Digital
 * B14 -> TSMS/CHG [in] ** Digital
 * A8  -> OK HS [in] ** Digital
 * A9  -> M LS [in] ** PWM in
 * B8, B9 -> CAN [in/out]
 * A2  -> AIR+ CNTL [out] ** Digital
 * A3  -> Prchg CNTL [out] ** Digital
 */

#define BMS_STATUS_Pin  GPIO_PIN_12
#define BMS_STATUS_Port GPIOB

#define IMD_STATUS_Pin  GPIO_PIN_13
#define IMD_STATUS_Port GPIOB

#define TSMS_CHG_Pin  GPIO_PIN_14
#define TSMS_CHG_Port GPIOB

#define OK_HS_Pin  GPIO_PIN_8
#define OK_HS_Port GPIOA

#define M_LS_Pin  GPIO_PIN_9
#define M_LS_Port GPIOA

#define CANR_Pin  GPIO_PIN_8
#define CANR_Port GPIOB

#define CANT_Pin  GPIO_PIN_9
#define CANT_Port GPIOB

#define AIR_Pin  GPIO_PIN_2
#define AIR_Port GPIOA

#define PCHG_Pin  GPIO_PIN_3
#define PCHG_Port GPIOA

#define LED_Pin       GPIO_PIN_13
#define LED_GPIO_Port GPIOC
