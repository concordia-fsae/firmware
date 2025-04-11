/**
 * @file torque.h
 * @brief Torque manager for vehicle control
 * @note Units for torque are in Nm
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
    TORQUE_INIT = 0x00U,
    TORQUE_INACTIVE,
    TORQUE_ACTIVE,
    TORQUE_ERROR,
} torque_state_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t                torque_getTorqueRequest(void);
torque_state_E           torque_getState(void);
CAN_torqueManagerState_E torque_getStateCAN(void);
