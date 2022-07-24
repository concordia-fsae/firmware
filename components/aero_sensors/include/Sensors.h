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
    NPA_Response_S npa;
    uint32_t pressure[MPRL_TOTAL_COUNT];
} Sensors_S;
