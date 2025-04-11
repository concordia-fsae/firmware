/**
* @file bppc.h
* @brief Module header for the Brake Pedal Plsuibility Check
 * @note Pedal positon is a float percentage between 0.0f and 1.0f where 
 *       0.0f is 0% and 1.0f is 100%
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
    BPPC_INIT = 0x00U,
    BPPC_OK,
    BPPC_FAULT,
    BPPC_FAULT_LATCHED,
    BPPC_ERROR, // Sensor failure
} bppc_state_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t       bppc_getPedalPosition(void);
bppc_state_E    bppc_getState(void);
CAN_bppcState_E bppc_getStateCAN(void);

