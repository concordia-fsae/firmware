/**
 * @file cockpit_lights.h
 * @brief Module that manages IMD and BMS lights 
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

CAN_digitalStatus_E cockpitLights_imd_getStateCAN(void);
CAN_digitalStatus_E cockpitLights_bms_getStateCAN(void);
