/**
 * app_faultManager.h
 * @brief Fault Manager header file
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// The fault index and fault messages are generated from yamcan
// TODO: Improve the generator
#include "CANTypes_generated.h"
#include "MessageUnpack_generated.h"

#include "LIB_Types.h"
#include "FeatureDefines_generated.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define MAX_FAULTS 64U

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

// Note: The following function must have a fault_index < 64U
#define app_faultManager_getNetworkedFault_state(bus, msg, fault_index) \
    (FLAG_get(CANRX_get_rawMessage(bus, msg)->u16, fault_index))
#define app_faultManager_getNetworkedFault_anySet(bus, msg) \
    (FLAG_any(CANRX_get_rawMessage(bus, msg)->u16, MAX_FAULTS))

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void     app_faultManager_setFaultState(FM_fault_E fault, bool faulted);
bool     app_faultManager_getFaultState(FM_fault_E fault);
uint8_t* app_faultManager_transmit(void);
