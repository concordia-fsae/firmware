/**
 * @file cockpit_lights.h
 * @brief Module that manages the horn
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/


//same includes as horn.c
#include "LIB_Types.h"
#include "CANTypes_generated.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    BMS_LIGHT_INIT = 0x00U,
    BMS_LIGHT_OFF,
    BMS_LIGHT_ON,
} BMS_LIGHT_state_E;

typedef enum
{
    IMD_LIGHT_INIT = 0x00U,
    IMD_LIGHT_OFF,
    IMD_LIGHT_ON,
} IMD_LIGHT_state_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

BMS_LIGHT_state_E       BMS_LIGHT_getState(void);
CAN_digitalStatus_E     cockpitLights_bms_getStateCAN(void);

IMD_LIGHT_state_E       IMD_LIGHT_getState(void);
CAN_digitalStatus_E     cockpitLights_imd_getStateCAN(void);
