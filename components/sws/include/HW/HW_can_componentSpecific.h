/**
 * @file HW_can_componentSpecific.h
 * @brief  Header file for component specific CAN firmware
 */

#pragma once

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    CAN_TX_MAILBOX_0 = 0U,
    CAN_TX_MAILBOX_1,
    CAN_TX_MAILBOX_2,
    CAN_TX_MAILBOX_COUNT,
} CAN_TxMailbox_E;

typedef enum
{
    CAN_RX_FIFO_0 = 0U,
    CAN_RX_FIFO_1,
    CAN_RX_FIFO_COUNT,
} CAN_RxFifo_E;
