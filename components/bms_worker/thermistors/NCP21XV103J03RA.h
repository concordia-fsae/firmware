/**
 * @file NCP21XV103J03RA.h
 * @brief  Header file for NCP21XV103J03RA thermistor
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Other Includes
#include "THERMISTORS.h"


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float NCP21_GetTempFromR_BParameter(THERM_BParameter_S* params, float r);
