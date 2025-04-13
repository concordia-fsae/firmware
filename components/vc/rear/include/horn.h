/**
 * @file horn.h
 * @brief Module that manages the horn
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"
#include "CANTypes_generated.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    HORN_INIT = 0x00U,
    HORN_OFF,
    HORN_ON,
} horn_state_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

horn_state_E        horn_getState(void);
CAN_digitalStatus_E horn_getStateCAN(void);
