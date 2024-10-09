/*
 * uds_componentSpecific.h
 * Component-specific defines for the UDS library
 */
#pragma once


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define UDS_ENABLE_LIB 1

#define ISOTP_TX_BUF_SIZE              128U  // [bytes] mostly arbitrary
#define ISOTP_RX_BUF_SIZE              128U  // [bytes] maximum size of a multi-frame ISOTP transaction (mostly arbitrary, increase if necessary)

#define UDS_RESPONSE_ID                0x123
#define UDS_REQUEST_ID                 0x456 // 0x124


// supported UDS services
#define UDS_SERVICE_SUPPORTED_ECU_RESET          true
#define UDS_SERVICE_SUPPORTED_ROUTINE_CONTROL    true
#define UDS_SERVICE_SUPPORTED_DID_READ           true
