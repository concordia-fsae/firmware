/*
 * CanTypes.h
 * This file contains types used for CAN messages and signals
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Types.h"
#include "NetworkDefines_generated.h"

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
} CAN_TxMessage_T;

typedef struct
{
    uint16_t                 id;

    CAN_IdentifierLen_E      IDE;
    CAN_RemoteTransmission_E RTR;

    uint8_t                  lengthBytes;
    CAN_data_T               data;

    uint16_t                 timestamp;
    uint8_t                  filterMatchIndex;
} CAN_RxMessage_T;

typedef bool (*packFn)(CAN_data_T *messsage, const uint8_t counter);

typedef struct
{
    const packFn   pack;
    const uint16_t id;
    const uint8_t  len;
} packTable_S;

typedef struct
{
    const packTable_S* const packTable;
    const uint8_t      packTableLength;
    const uint16_t     period;
    uint8_t            counter;
    uint8_t            index;
    uint32_t           lastTimestamp;
} busTable_S;

typedef struct
{
    busTable_S* const busTable;
    const uint8_t     busTableLength;
} canTable_S;
