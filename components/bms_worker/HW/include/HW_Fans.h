/**
 * @file HW_Fans.h
 * @brief  Segment Fans Control Driver Header
 */

#pragma once


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Inlcudes
#include "stdbool.h"
#include "stdint.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    FANS_OFF = 0x00,
    FANS_STARTING,
    FANS_RUNNING,
} FANS_State_E;

typedef enum
{
    FAN1 = 0x00,
    FAN2,
    FAN_COUNT,
} Fan_E;

typedef struct
{
    FANS_State_E current_state;
} FANS_S;


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

extern FANS_S FANS;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool         FANS_init(void);
void         FANS_setPower(uint8_t* percentage);
void         FANS_getRPM(uint16_t* rpm);

static inline FANS_State_E FANS_getState()
{
    return FANS.current_state;
}
