/**
 * @file HW_NX3L4051PW.h
 * @brief  Header file for NX3L4051PW Driver
 */

# pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
# include "stdbool.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    NX3L_MUX1 = 0x00,
    NX3L_MUX2,
    NX3L_MUX3,
    NX3L_MUX4,
    NX3L_MUX5,
    NX3L_MUX6,
    NX3L_MUX7,
    NX3L_MUX8,
    NX3L_MUX_COUNT,
} NX3L_MUXChannel_E;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool NX3L_init(void);
bool NX3L_setMux(NX3L_MUXChannel_E chn);
bool NX3L_enableMux(void);
bool NX3L_disableMux(void);
