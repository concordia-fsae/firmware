/**
 * @file MCFAULT.h
 * @brief Module that manages motor controller faults
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"
#include "CANTypes_generated.h"

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

CAN_faultStatus_E MCFAULT_getFaultedCAN(void);
CAN_faultStatus_E MCFAULT_getTimedOutCAN(void);