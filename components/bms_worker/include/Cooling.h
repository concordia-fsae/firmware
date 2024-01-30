/**
 * @file Cooling.h
 * @brief  Header file for Cooling Application
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "stdint.h"

// Driver Includes
#include "HW_Fans.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    COOLING_INIT = 0x00,
    COOLING_OFF,
    COOLING_ON,
    COOLING_FULL,
    COOLING_ERR,
} Cooling_State_E;

typedef struct
{
    Cooling_State_E state[FAN_COUNT];
    uint8_t         percentage[FAN_COUNT];
    uint16_t        rpm[FAN_COUNT];
} Cooling_Mngr_S;


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern Cooling_Mngr_S COOLING;
