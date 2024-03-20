/**
 * @file THERMISTORS.h
 * @brief  Header file for Thermistor Library
 */

#pragma once


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define KELVIN_OFFSET 273.15F


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float B;
    float T0;
    float R0;
} THERM_BParameter_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float THERM_GetTempFromR_BParameter(THERM_BParameter_S* params, float r);
