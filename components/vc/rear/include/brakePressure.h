/**
 * @file brakePressure.h
 * @brief Module header for brake pressure sensor
 */

#pragma once

#include "Module.h"
#include "ModuleDesc.h"
#include "drv_inputAD_componentSpecific.h"
#include "MessageUnpack_generated.h"


/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t brakePressure_getBrakePressure(void);
