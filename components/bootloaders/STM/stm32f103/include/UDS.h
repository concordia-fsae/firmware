/*
 * UDS.h
 * UDS module header file
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// component-specific UDS configuration
#include "uds_componentSpecific.h"

// other includes
#include "Types.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define UDS_STARTUP_WAIT        50U   // [ms] how long to wait for a TP frame at startup
                                      // before booting
#define UDS_INACTIVITY_TIMER    100U  // [ms] how long to wait without a TP frame
                                      // before booting


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void UDS_init(void);
void UDS_periodic_1kHz(void);
bool UDS_shouldInhibitBoot(void);
