/**
 * @file mcManager.h
 * @brief Header file for the motor controller manager
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
    MCMANAGER_FORWARD = 0x00,
    MCMANAGER_REVERSE,
} mcManager_direction_E;

typedef enum
{
    MCMANAGER_DISABLE = 0x00,
    MCMANAGER_ENABLE,
} mcManager_enable_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t                     mcManager_getTorqueCommand(void);
CAN_pm100dxDirectionCommand_E mcManager_getDirectionCommand(void);
CAN_pm100dxEnableState_E      mcManager_getEnableCommand(void);
float32_t                     mcManager_getTorqueLimit(void);
bool                          mcManager_clearEepromCommand(void);
