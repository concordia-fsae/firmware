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

#define CAN_requestIdHelper(nodeId) (JOIN3(CAN_VEH_UDSCLIENT_bmsw, nodeId, UdsRequest_ID))
#define UDS_REQUEST_ID CAN_requestIdHelper(CAN_NODE_OFFSET)
#define UDS_RESPONSE_ID CAN_VEH_BMSW_udsResponse_ID

#define ISOTP_TX_BUF_SIZE              128U  // [bytes] mostly arbitrary
#define ISOTP_RX_BUF_SIZE              128U  // [bytes] maximum size of a multi-frame ISOTP transaction (mostly arbitrary, increase if necessary)

// supported UDS services
#define UDS_SERVICE_SUPPORTED_ECU_RESET          true
#define UDS_SERVICE_SUPPORTED_ROUTINE_CONTROL    true
#define UDS_SERVICE_SUPPORTED_DID_READ           true
#endif // FEATURE_UDS
