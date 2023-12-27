/**
 * @file HW_Fans.h
 * @brief  Segment Fans Control Driver Header
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-18
 */

#pragma once


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Utility.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum {
    OFF = 0x00,
    STARTING,
    RUNNING,
    FULL
} FANS_State_E;


/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool FANS_Init(void);
bool FANS_Verify(void);
FANS_State_E FANS_GetState(void);
void FANS_SetPower(uint8_t percentage);