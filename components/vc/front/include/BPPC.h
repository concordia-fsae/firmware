/**
* @file BPPC.h
* @brief Module header for the Brake Pedal Plausibility Check
*/

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CANTypes_generated.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    BPPC_INIT = 0x00U,
    BPPC_OK,
    BPPC_INPLAUSIBLE,
    BPPC_FAULT,
    BPPC_ERROR,
} BPPC_state_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

BPPC_state_E    BPPC_getState(void);
CAN_bppcState_E BPPC_getStateCAN(void);
