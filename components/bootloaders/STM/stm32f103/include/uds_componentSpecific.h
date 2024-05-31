/*
 * uds_componentSpecific.h
 * Component-specific defines for the UDS library
 */
#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "BuildDefines_generated.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ISOTP_TX_BUF_SIZE              128U  // [bytes] mostly arbitrary
#define ISOTP_RX_BUF_SIZE              128U  // [bytes] maximum size of a multi-frame ISOTP transaction (mostly arbitrary, increase if necessary)


#define UDS_DOWNLOAD_MAX_BLOCK_SIZE    8U    // [bytes] maximum size of each packet during UDS download
#define UDS_DOWNLOAD_USE_CRC           true  // whether each download block has a CRC appended
#define UDS_DOWNLOAD_CRC_SIZE          8U    // [bits] size of the CRC in each download block


// supported UDS services
#define UDS_SERVICE_SUPPORTED_ECU_RESET          true
#define UDS_SERVICE_SUPPORTED_ROUTINE_CONTROL    true
#define UDS_SERVICE_SUPPORTED_DOWNLOAD           true
#define UDS_SERVICE_SUPPORTED_DID_READ           true
