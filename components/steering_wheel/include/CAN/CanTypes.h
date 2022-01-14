/*
 * CanTypes.h
 * This file contains types used for CAN messages and signals
 */
#pragma once

#include "Types.h"

typedef enum
{
    CAN_MAILBOX_0 = 0U,
    CAN_MAILBOX_1,
    CAN_MAILBOX_2,
    CAN_MAILBOX_COUNT,
} CAN_TxMailbox_E;

typedef union
{
    uint64_t data64;
    uint32_t data32[2];
    uint16_t data16[4];
    uint8_t  data8[8];

} CAN_data_U;

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

typedef struct
{
    uint16_t                 id;

    CAN_IdentifierLen_E      IDE;
    CAN_RemoteTransmission_E RTR;

    uint8_t                  lengthBytes;
    CAN_data_U               data;
    CAN_TxMailbox_E          mailbox;
} CAN_TxMessage_t;
