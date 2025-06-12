/**
 * @file tssi.h
 * @brief Module that manages the tssi
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
    TSSI_INIT = 0x00U,
    TSSI_ON_GREEN,
    TSSI_ON_RED,
} tssi_state_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

tssi_state_E    tssi_getState(void);
CAN_tssiState_E tssi_getStateCAN(void);
