/**
* @file brakeTemp.h
 * @brief Module header for brake Temp sensor
 */

#pragma once

#include "Module.h"
#include "LIB_Types.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t brakeTemp_getFLTemp(void); //returns FLtemp
float32_t brakeTemp_getFRTemp(void);

float32_t brakeTemp_getFLVoltage(void);
float32_t brakeTemp_getFRVoltage(void);
