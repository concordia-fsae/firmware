/*
 * CanTypes.h
 * This file contains types used for CAN messages and signals
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

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

typedef enum
{
    CAN_BAUDRATE_1MBIT = 0U,
    CAN_BAUDRATE_500KBIT,
} CAN_baudrate_E;

typedef union
{
    uint64_t u64;
    uint32_t u32[2];
    uint16_t u16[4];
    uint8_t  u8[8];
} CAN_data_T;

typedef struct
{
    CAN_baudrate_E baudrate;
} CAN_busConfig_T;

typedef struct
{
    uint32_t                 id;

    CAN_IdentifierLen_E      IDE;
    CAN_RemoteTransmission_E RTR;

    uint8_t                  lengthBytes;
    CAN_data_T               data;
} CAN_TxMessage_T;

typedef struct
{
    uint32_t                 id;

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
    const uint32_t id;
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
