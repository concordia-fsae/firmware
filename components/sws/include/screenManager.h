/**
 * @file screenManeger.h
 * @brief Manage dynamic screen displays
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stdbool.h"
#include "CANTypes_generated.h"

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

CAN_screenAlerts_E   screenManager_getAlertCAN(void);
CAN_screenWarnings_E screenManager_getWarningCAN(void);
