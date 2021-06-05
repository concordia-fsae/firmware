/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stdbool.h"
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
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
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
