/**
  ******************************************************************************
  * @file    can.h
  * @brief   This file contains all the function prototypes for
  *          the can.c file
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
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "SystemConfig.h"


extern CAN_HandleTypeDef hcan;


// which CAN interrupts we want to enable
#define CAN_ENABLED_INTERRUPTS (CAN_IER_TMEIE | CAN_IER_FMPIE0 | \
                                CAN_IER_FMPIE1 | CAN_IER_FFIE0 | \
                                CAN_IER_FFIE1 | CAN_IER_FOVIE0 | \
                                CAN_IER_FOVIE1 | CAN_IER_WKUIE | \
                                CAN_IER_SLKIE | CAN_IER_EWGIE | \
                                CAN_IER_EPVIE | CAN_IER_BOFIE | \
                                CAN_IER_LECIE | CAN_IER_ERRIE)

void MX_CAN_Init(void);

void CAN_Error_SWI(uint16_t errorId);
void CAN_ErrorUnknown_SWI(CAN_HandleTypeDef *canHandle);
void CAN_ErrorTx_SWI(CAN_HandleTypeDef *canHandle);
void CAN_ErrorFIFOFull_SWI(CAN_HandleTypeDef *canHandle);

void CAN_TxComplete_SWI(uint8_t mailboxId);
void CAN_TxComplete_MB0_SWI(CAN_HandleTypeDef *canHandle);
void CAN_TxComplete_MB1_SWI(CAN_HandleTypeDef *canHandle);
void CAN_TxComplete_MB2_SWI(CAN_HandleTypeDef *canHandle);

void CAN_RxMsgPending_SWI(uint8_t fifoId);
void CAN_RxMsgPending_FIFO0_SWI(CAN_HandleTypeDef *canHandle);
void CAN_RxMsgPending_FIFO1_SWI(CAN_HandleTypeDef *canHandle);

#ifdef __cplusplus
}
#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
