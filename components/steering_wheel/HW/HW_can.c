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
#include "IO.h"

CAN_HandleTypeDef hcan;

/* CAN init function */
void MX_CAN_Init(void)
{
    hcan.Instance                  = CAN1;
    hcan.Init.Prescaler            = 4;
    hcan.Init.Mode                 = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth        = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1             = CAN_BS1_6TQ;
    hcan.Init.TimeSeg2             = CAN_BS2_1TQ;
    hcan.Init.TimeTriggeredMode    = DISABLE;
    hcan.Init.AutoBusOff           = DISABLE;
    hcan.Init.AutoWakeUp           = DISABLE;
    hcan.Init.AutoRetransmission   = DISABLE;
    hcan.Init.ReceiveFifoLocked    = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;

    if (HAL_CAN_Init(&hcan) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_CAN_ActivateNotification(&hcan, CAN_ENABLED_INTERRUPTS);

    hcan.TxMailbox0CompleteCallback = CAN_TxComplete_MB0_SWI;        // CAN Tx Mailbox 0 complete callback
    hcan.TxMailbox1CompleteCallback = CAN_TxComplete_MB1_SWI;        // CAN Tx Mailbox 1 complete callback
    hcan.TxMailbox2CompleteCallback = CAN_TxComplete_MB2_SWI;        // CAN Tx Mailbox 2 complete callback
    hcan.TxMailbox0AbortCallback    = CAN_TxError_SWI;               // CAN Tx Mailbox 0 abort callback
    hcan.TxMailbox1AbortCallback    = CAN_TxError_SWI;               // CAN Tx Mailbox 1 abort callback
    hcan.TxMailbox2AbortCallback    = CAN_TxError_SWI;               // CAN Tx Mailbox 2 abort callback
    hcan.RxFifo0MsgPendingCallback  = CAN_RxMsgPending_FIFO0_SWI;    // CAN Rx FIFO 0 msg pending callback
    hcan.RxFifo0FullCallback        = CAN_ErrorFIFOFull_SWI;         // CAN Rx FIFO 0 full callback
    hcan.RxFifo1MsgPendingCallback  = CAN_RxMsgPending_FIFO1_SWI;    // CAN Rx FIFO 1 msg pending callback
    hcan.RxFifo1FullCallback        = CAN_ErrorFIFOFull_SWI;         // CAN Rx FIFO 1 full callback
    hcan.SleepCallback              = NULL;                          // CAN Sleep callback
    hcan.WakeUpFromRxMsgCallback    = NULL;                          // CAN Wake Up from Rx msg callback
    hcan.ErrorCallback              = CAN_ErrorUnknown_SWI;          // CAN Error callback
}

/*
 * This SWI is called whenever a CAN mailbox is free. It should then push a
 * messge from the queue for that mailbox into the mailbox
 */
void CAN_TxComplete_SWI(uint8_t mailbox)
{
    IO.test++;
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


void CAN_ErrorUnknown_SWI(CAN_HandleTypeDef* canHandle)
{
    UNUSED(canHandle);
}


void CAN_TxError_SWI(CAN_HandleTypeDef* canHandle)
{
    UNUSED(canHandle);
}


void CAN_ErrorFIFOFull_SWI(CAN_HandleTypeDef* canHandle)
{
    UNUSED(canHandle);
}


void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{
    if (canHandle->Instance == CAN1)
    {
        /* CAN1 clock enable */
        __HAL_RCC_CAN1_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();
        /**CAN GPIO Configuration
        PA11     ------> CAN_RX
        PA12     ------> CAN_TX
        */
        GPIO_InitTypeDef GPIO_InitStruct = { 0 };

        GPIO_InitStruct.Pin  = GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        GPIO_InitStruct.Pin   = GPIO_PIN_12;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 0, 0U);
        HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);

        HAL_NVIC_SetPriority(CAN1_TX_IRQn, 0, 0U);
        HAL_NVIC_EnableIRQ(CAN1_TX_IRQn);

        HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 0, 0U);
        HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);

        HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 0, 0U);
        HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
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

        HAL_NVIC_DisableIRQ(CAN1_SCE_IRQn);
        HAL_NVIC_DisableIRQ(CAN1_TX_IRQn);
        HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn);
        HAL_NVIC_DisableIRQ(CAN1_RX1_IRQn);
    }
}

HAL_StatusTypeDef CAN_sendMsg(CAN_HandleTypeDef* canHandle, CAN_TxMessage_t msg)
{
    HAL_CAN_StateTypeDef state = canHandle->State;
    uint32_t             tsr   = READ_REG(canHandle->Instance->TSR);

    if ((state == HAL_CAN_STATE_READY) ||
        (state == HAL_CAN_STATE_LISTENING))
    {
        // check that the specified mailbox is free
        bool mailboxFree = false;
        switch(msg.mailbox)
        {
            case CAN_MAILBOX_0:
                mailboxFree = (tsr & CAN_TSR_TME0) != 0U;
                break;
            case CAN_MAILBOX_1:
                mailboxFree = (tsr & CAN_TSR_TME1) != 0U;
                break;
            case CAN_MAILBOX_2:
                mailboxFree = (tsr & CAN_TSR_TME2) != 0U;
                break;
            case CAN_MAILBOX_COUNT:
            default:
                break;
        }

        if (mailboxFree)
        {

            // set CAN ID
            canHandle->Instance->sTxMailBox[msg.mailbox].TIR = ((msg.id << CAN_TI0R_STID_Pos) | msg.RTR);
            // set message length
            canHandle->Instance->sTxMailBox[msg.mailbox].TDTR = msg.lengthBytes;

            // set message data
            canHandle->Instance->sTxMailBox[msg.mailbox].TDHR = msg.data.data32[1];
            canHandle->Instance->sTxMailBox[msg.mailbox].TDLR = msg.data.data32[0];
            // TODO: test whether WRITE_REG compiles down to a different instruction than
            // just directly setting the register
            // WRITE_REG(hcan->Instance->sTxMailBox[msg.mailbox].TDHR,
            //           ((uint32_t)aData[7] << CAN_TDH0R_DATA7_Pos) |
            //               ((uint32_t)aData[6] << CAN_TDH0R_DATA6_Pos) |
            //               ((uint32_t)aData[5] << CAN_TDH0R_DATA5_Pos) |
            //               ((uint32_t)aData[4] << CAN_TDH0R_DATA4_Pos));
            // WRITE_REG(hcan->Instance->sTxMailBox[msg.mailbox].TDLR,
            //           ((uint32_t)aData[3] << CAN_TDL0R_DATA3_Pos) |
            //               ((uint32_t)aData[2] << CAN_TDL0R_DATA2_Pos) |
            //               ((uint32_t)aData[1] << CAN_TDL0R_DATA1_Pos) |
            //               ((uint32_t)aData[0] << CAN_TDL0R_DATA0_Pos));


            // request message transmission
            SET_BIT(canHandle->Instance->sTxMailBox[msg.mailbox].TIR, CAN_TI0R_TXRQ);

            /* Return function status */
            return HAL_OK;
        }
        else
        {
            // update error to show that no mailbox was free
            canHandle->ErrorCode |= HAL_CAN_ERROR_PARAM;

            return HAL_ERROR;
        }
    }
    else
    {
        // update error to show that peripheral was in the wrong state for transmission
        canHandle->ErrorCode |= HAL_CAN_ERROR_NOT_INITIALIZED;

        return HAL_ERROR;
    }
}
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
