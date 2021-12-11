/**
 ******************************************************************************
 * @file    can.c
 * @brief   This file provides code for the configuration
 *          of the CAN instances.
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

/* Includes ------------------------------------------------------------------*/
#include "HW_can.h"

CAN_HandleTypeDef hcan;

/* CAN init function */
void MX_CAN_Init(void)
{
    hcan.Instance                   = CAN1;
    hcan.Init.Prescaler             = 4;
    hcan.Init.Mode                  = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth         = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1              = CAN_BS1_6TQ;
    hcan.Init.TimeSeg2              = CAN_BS2_1TQ;
    hcan.Init.TimeTriggeredMode     = DISABLE;
    hcan.Init.AutoBusOff            = DISABLE;
    hcan.Init.AutoWakeUp            = DISABLE;
    hcan.Init.AutoRetransmission    = DISABLE;
    hcan.Init.ReceiveFifoLocked     = DISABLE;
    hcan.Init.TransmitFifoPriority  = DISABLE;
    hcan.TxMailbox0CompleteCallback = CAN_TxComplete_MB0_SWI;        // CAN Tx Mailbox 0 complete callback
    hcan.TxMailbox1CompleteCallback = CAN_TxComplete_MB1_SWI;        // CAN Tx Mailbox 1 complete callback
    hcan.TxMailbox2CompleteCallback = CAN_TxComplete_MB2_SWI;        // CAN Tx Mailbox 2 complete callback
    hcan.TxMailbox0AbortCallback    = CAN_ErrorTx_SWI;               // CAN Tx Mailbox 0 abort callback
    hcan.TxMailbox1AbortCallback    = CAN_ErrorTx_SWI;               // CAN Tx Mailbox 1 abort callback
    hcan.TxMailbox2AbortCallback    = CAN_ErrorTx_SWI;               // CAN Tx Mailbox 2 abort callback
    hcan.RxFifo0MsgPendingCallback  = CAN_RxMsgPending_FIFO0_SWI;    // CAN Rx FIFO 0 msg pending callback
    hcan.RxFifo0FullCallback        = CAN_ErrorFIFOFull_SWI;         // CAN Rx FIFO 0 full callback
    hcan.RxFifo1MsgPendingCallback  = CAN_RxMsgPending_FIFO1_SWI;    // CAN Rx FIFO 1 msg pending callback
    hcan.RxFifo1FullCallback        = CAN_ErrorFIFOFull_SWI;         // CAN Rx FIFO 1 full callback
    hcan.SleepCallback              = NULL;                          // CAN Sleep callback
    hcan.WakeUpFromRxMsgCallback    = NULL;                          // CAN Wake Up from Rx msg callback
    hcan.ErrorCallback              = CAN_ErrorUnknown_SWI;          // CAN Error callback

    if (HAL_CAN_Init(&hcan) != HAL_OK)
    {
        Error_Handler();
    }
}

/*
 * This SWI is called whenever a CAN mailbox is free. It should then push a
 * messge from the queue for that mailbox into the mailbox
 */
void CAN_TxComplete_SWI(uint8_t mailbox)
{
    UNUSED(mailbox);
}


/*
 * This SWI is called whenever a CAN message is received. It should then call
 * the relevant Rx function
 */
void CAN_RxMsgPending_SWI(uint8_t mailboxId)
{
    UNUSED(mailboxId);
}


/*
 * This SWI is called whenever there is a CAN error. It should eventually do
 * something, though not sure what yet
 */
void CAN_Error_SWI(uint16_t errorId)
{
    UNUSED(errorId);
}


void CAN_TxComplete_MB0_SWI(CAN_HandleTypeDef* canHandle)
{
    UNUSED(canHandle);
    CAN_TxComplete_SWI(0U);
}


void CAN_TxComplete_MB1_SWI(CAN_HandleTypeDef* canHandle)
{
    UNUSED(canHandle);
    CAN_TxComplete_SWI(1U);
}


void CAN_TxComplete_MB2_SWI(CAN_HandleTypeDef* canHandle)
{
    UNUSED(canHandle);
    CAN_TxComplete_SWI(2U);
}


void CAN_RxMsgPending_FIFO0_SWI(CAN_HandleTypeDef* canHandle)
{
    UNUSED(canHandle);
    CAN_RxMsgPending_SWI(0U);
}


void CAN_RxMsgPending_FIFO1_SWI(CAN_HandleTypeDef* canHandle)
{
    UNUSED(canHandle);
    CAN_RxMsgPending_SWI(1U);
}


void CAN_ErrorUnknown_SWI(CAN_HandleTypeDef *canHandle)
{
    UNUSED(canHandle);
}


void CAN_ErrorTx_SWI(CAN_HandleTypeDef *canHandle)
{
    UNUSED(canHandle);
}


void CAN_ErrorFIFOFull_SWI(CAN_HandleTypeDef *canHandle)
{
    UNUSED(canHandle);
}


void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    if (canHandle->Instance == CAN1)
    {
        /* CAN1 clock enable */
        __HAL_RCC_CAN1_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();
        /**CAN GPIO Configuration
        PA11     ------> CAN_RX
        PA12     ------> CAN_TX
        */
        GPIO_InitStruct.Pin  = GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        GPIO_InitStruct.Pin   = GPIO_PIN_12;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{
    if (canHandle->Instance == CAN1)
    {
        /* Peripheral clock disable */
        __HAL_RCC_CAN1_CLK_DISABLE();

        /**CAN GPIO Configuration
        PA11     ------> CAN_RX
        PA12     ------> CAN_TX
        */
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
    }
}
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
