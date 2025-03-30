/**
 * @file cooling.h
 * @brief  Header file for Cooling Application
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "app_cooling.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    COOLING_CHANNEL_FAN1,
    COOLING_CHANNEL_FAN2,
    COOLING_CHANNEL_COUNT,
} cooling_channel_E;

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern app_cooling_channel_S cooling[COOLING_CHANNEL_COUNT];
