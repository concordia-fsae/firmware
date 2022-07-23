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

#define NPA_USE_TEMPERATURE 2 /**< Defines usage of NPA temperature readings (2: 12bit; 1: 8bit, 0: None) */

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint8_t status;
    uint16_t pressure;
#if NPA_USE_TEMPERATURE == 1
    uint8_t temperature;
#elif NPA_USE_TEMPERATURE == 2
    uint16_t temperature;
#endif
} NPA_Response_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

NPA_Response_S NPA_Read(void);
