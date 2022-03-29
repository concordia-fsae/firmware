/*
 * CanTypes.h
 * This file contains types used for CAN messages and signals
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Types.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    CAN_TX_PRIO_1KHZ = 0U,
    CAN_TX_PRIO_100HZ,
    CAN_TX_PRIO_10HZ,
    CAN_TX_PRIO_COUNT,
} CAN_TX_Priorities_E;

typedef enum
{
    CAN_TX_MAILBOX_0 = 0U,
    CAN_TX_MAILBOX_1,
    CAN_TX_MAILBOX_2,
    CAN_TX_MAILBOX_COUNT,
} CAN_TxMailbox_E;

_Static_assert((uint8_t)CAN_TX_PRIO_COUNT == (uint8_t)CAN_TX_MAILBOX_COUNT, "Number of TX priorities should equal the number of TX mailboxes");

typedef enum
{
    CAN_RX_FIFO_0 = 0U,
    CAN_RX_FIFO_1,
    CAN_RX_FIFO_COUNT,
} CAN_RxFifo_E;

typedef enum
{
    CAN_IDENTIFIER_STD = 0U,    // Standard length CAN ID
    CAN_IDENTIFIER_EXT,         // Extended length CAN ID
} CAN_IdentifierLen_E;

typedef enum
{
    CAN_REMOTE_TRANSMISSION_REQUEST_DATA = 0U,
    CAN_REMOTE_TRANSMISSION_REQUEST_REMOTE,
} CAN_RemoteTransmission_E;


typedef union
{
    uint64_t u64;
    uint32_t u32[2];
    uint16_t u16[4];
    uint8_t  u8[8];
} CAN_data_T;

typedef struct
{
    uint16_t                 id;

    CAN_IdentifierLen_E      IDE;
    CAN_RemoteTransmission_E RTR;

    uint8_t                  lengthBytes;
    CAN_data_T               data;
    CAN_TxMailbox_E          mailbox;
} CAN_TxMessage_t;

typedef struct
{
    bool (*pack) (CAN_data_T *message, const int counter);
    uint16_t id;
    uint8_t  len;
} packTable_S;
