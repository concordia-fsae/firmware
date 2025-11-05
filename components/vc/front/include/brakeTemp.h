/**
* @file brakeTemp
* @brief Module that manages the front brake temperature sensors
*/

#pragma once

/******************************************************************************
*                             I N C L U D E S
******************************************************************************/

#include "Module.h"
#include "LIB_Types.h"

/******************************************************************************
*            P U B L I C  F U N C T I O N  P R O T O T Y P E S
******************************************************************************/

float32_t brakeTemp_getFLtemp(void);
float32_t brakeTemp_getFLVoltage(void);

float32_t brakeTemp_getFRtemp(void);
float32_t brakeTemp_getFRVoltage(void);