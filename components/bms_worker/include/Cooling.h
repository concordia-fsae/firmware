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
    COOL_INIT = 0x00,
    COOL_OFF,
    COOL_ON,
    COOL_FULL,
    COOL_ERR,
} COOL_state_E;

typedef struct
{
    COOL_state_E state[FAN_COUNT];
    uint8_t         percentage[FAN_COUNT];
    uint16_t        rpm[FAN_COUNT];
} COOL_S;


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern COOL_S COOL;
