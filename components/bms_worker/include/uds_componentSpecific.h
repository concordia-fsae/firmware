/*
 * uds_componentSpecific.h
 * Component-specific defines for the UDS library
 */
#pragma once

#include "NetworkDefines_generated.h"
#include "FeatureDefines_generated.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(APP_UDS)
#define UDS_ENABLE_LIB  APP_UDS

#ifdef CAN_VEH_UDSCLIENT_bmsw0UdsRequest_ID
#define UDS_REQUEST_ID CAN_VEH_UDSCLIENT_bmsw0UdsRequest_ID
#elif defined(CAN_VEH_UDSCLIENT_bmsw1UdsRequest_ID)
#define UDS_REQUEST_ID CAN_VEH_UDSCLIENT_bmsw1UdsRequest_ID
#elif defined(CAN_VEH_UDSCLIENT_bmsw2UdsRequest_ID)
#define UDS_REQUEST_ID CAN_VEH_UDSCLIENT_bmsw2UdsRequest_ID
#elif defined(CAN_VEH_UDSCLIENT_bmsw3UdsRequest_ID)
#define UDS_REQUEST_ID CAN_VEH_UDSCLIENT_bmsw3UdsRequest_ID
#elif defined(CAN_VEH_UDSCLIENT_bmsw4UdsRequest_ID)
#define UDS_REQUEST_ID CAN_VEH_UDSCLIENT_bmsw4UdsRequest_ID
#elif defined(CAN_VEH_UDSCLIENT_bmsw5UdsRequest_ID)
#define UDS_REQUEST_ID CAN_VEH_UDSCLIENT_bmsw5UdsRequest_ID
#endif
#ifdef CAN_VEH_UDSCLIENT_bmsw0UdsRequest_ID
#define UDS_RESPONSE_ID CAN_VEH_BMSW0_udsResponse_ID
#elif defined(CAN_VEH_UDSCLIENT_bmsw1UdsRequest_ID)
#define UDS_RESPONSE_ID CAN_VEH_BMSW1_udsResponse_ID
#elif defined(CAN_VEH_UDSCLIENT_bmsw2UdsRequest_ID)
#define UDS_RESPONSE_ID CAN_VEH_BMSW2_udsResponse_ID
#elif defined(CAN_VEH_UDSCLIENT_bmsw3UdsRequest_ID)
#define UDS_RESPONSE_ID CAN_VEH_BMSW3_udsResponse_ID
#elif defined(CAN_VEH_UDSCLIENT_bmsw4UdsRequest_ID)
#define UDS_RESPONSE_ID CAN_VEH_BMSW4_udsResponse_ID
#elif defined(CAN_VEH_UDSCLIENT_bmsw5UdsRequest_ID)
#define UDS_RESPONSE_ID CAN_VEH_BMSW5_udsResponse_ID
#endif

#define ISOTP_TX_BUF_SIZE              128U  // [bytes] mostly arbitrary
#define ISOTP_RX_BUF_SIZE              128U  // [bytes] maximum size of a multi-frame ISOTP transaction (mostly arbitrary, increase if necessary)

// supported UDS services
#define UDS_SERVICE_SUPPORTED_ECU_RESET          true
#define UDS_SERVICE_SUPPORTED_ROUTINE_CONTROL    true
#define UDS_SERVICE_SUPPORTED_DID_READ           true
#endif // FEATURE_UDS
