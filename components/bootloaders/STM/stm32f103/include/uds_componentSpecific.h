/*
 * uds_componentSpecific.h
 * Component-specific defines for the UDS library
 */
#pragma once

#define ISOTP_TX_BUF_SIZE    128U  // mostly arbitrary
#define ISOTP_RX_BUF_SIZE    4096U // maximum multi-frame isotp transmission is 4096

#define UDS_RESPONSE_ID      0x123
#define UDS_REQUEST_ID       0x456 // 0x124


// supported UDS services
#define UDS_SERVICE_SUPPORTED_ECU_RESET          true
#define UDS_SERVICE_SUPPORTED_ROUTINE_CONTROL    true
