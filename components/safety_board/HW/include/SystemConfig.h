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

/**
 * Pin Description
 * B12 -> BMS Status [out] ** Digital
 * B13 -> IMD Status [out] ** Digital
 * B14 -> TSMS/CHG [in] ** Digital
 * A8  -> OK HS [in] ** Digital
 * A9  -> M LS [in] ** PWM in
 * B8, B9 -> CAN [in/out]
 * A2  -> AIR+ CNTL [out] ** Digital
 * A3  -> Prchg CNTL [out]
 */

#define BMS_STATUS_Pin GPIO_PIN_12
#define BMS_STATUS_Port GPIOB

#define IMD_STATUS_Pin GPIO_PIN_13
#define IMD_STATUS_Port GPIOB

#define TSMS_CHG_Pin GPIO_PIN_14
#define TSMS_CHG_Port GPIOB

#define OK_HS_Pin GPIO_PIN_8
#define OK_HS_Port GPIOA

#define M_LS_Pin GPIO_PIN_9
#define M_LS_Port GPIOA

#define CANR_Pin GPIO_PIN_8
#define CANR_Port GPIOB

#define CANT_Pin GPIO_PIN_9
#define CANT_Port GPIOB

#define AIR_Pin GPIO_PIN_2
#define AIR_Port GPIOA

#define PCHG_Pin GPIO_PIN_3
#define PCHG_Port GPIOA
