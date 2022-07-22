/**
 * @file HW_NPA.h
 * @brief  Header file of NPA-700B-030D driver
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-22
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_i2c.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define NPA_BUS_TIMEOUT 5


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint8_t status;
    uint16_t pressure;
} NPA_Response_S;

typedef struct
{
    NPA_Response_S response;
    uint8_t temp;
} NPA_Full_Response8_S;

typedef struct
{
    NPA_Response_S response;
    uint16_t temp;
} NPA_Full_Response16_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

NPA_Response_S NPA_Read(void);
NPA_Full_Response8_S NPA_ReadTemp8(void);
NPA_Full_Response16_S NPA_ReadTemp16(void);
