/**
 * @file Sensors.h
 * @brief  Header file for the ARS sensors
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-23
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_NPA.h"
#include "HW_MPRL.h"
#include "HW_MAX7356.h"
#include "ModuleDesc.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct 
{
    uint32_t timestamp;
    uint16_t diff_pressure;
#if NPA_USE_TEMPERATURE == RECORD_8BIT_TEMPERATURE
    uint8_t temperature;
#elif NPA_USE_TEMPERATURE == RECORD_12BIT_TEMPERATURE
    uint16_t temperature;
#endif
    uint32_t pressure[MPRL_TOTAL_COUNT];
} Sensors_S;
