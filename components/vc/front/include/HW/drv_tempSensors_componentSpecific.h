/**
 * @file drv_tempSensors_componentSpecific.h
 * @brief Header file for the VCFRONT component specific temperature sensors
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_tempSensors.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_TEMPSENSOR_L_BR_TEMP = 0x00,
    DRV_TEMPSENSOR_R_BR_TEMP,
    DRV_TEMPSENSORS_CHANNEL_COUNT
} drv_tempSensors_channel_E;