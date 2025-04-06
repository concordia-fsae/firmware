/**
 * @file drv_pedalMonitor_componentSpecific.h
 * @brief Header file for the VCFRONT component specific pedal monitor
 * @note Pedal positon is a float percentage between 0.0f and 1.0f where 
 *       0.0f is 0% and 1.0f is 100%
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_pedalMonitor.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_PEDALMONITOR_APPS1 = 0x00,
    DRV_PEDALMONITOR_APPS2,
    DRV_PEDALMONITOR_BRAKE_POT,
    DRV_PEDALMONITOR_CHANNEL_COUNT
} drv_pedalMonitor_channel_E;
