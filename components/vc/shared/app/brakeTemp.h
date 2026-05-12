/**
 * @file brakeTemp.h
 * @brief Module header for brake temp sensors
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"

/******************************************************************************
*                             T Y P E D E F S
 ******************************************************************************/

typedef enum {
    BRAKETEMP_LEFT,
    BRAKETEMP_RIGHT,
    BRAKETEMP_COUNT
} brakeTemp_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t brakeTemp_getTemperature(brakeTemp_E temp);
float32_t brakeTemp_getVoltage(brakeTemp_E temp);

