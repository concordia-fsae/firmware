/**
 * @file HW_MPRL.h
 * @brief  Header file of MPRLS0025PA00001A driver
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-21
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_i2c.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define MPRL_BUS1_COUNT 8
#define MPRL_BUS2_COUNT 8
#define MPRL_TOP_OFFSET 0
#define MPRL_BOT_OFFSET MPRL_BUS1_COUNT
#define MPRL_TOTAL_COUNT MPRL_BUS1_COUNT + MPRL_BUS2_COUNT
#define MPRL_BUS_TIMEOUT 5

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct {
    uint8_t status;
    uint32_t data;
} MPRL_Response_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void MPRL_StartConversion(void);
uint8_t MPRL_ReadStatus(I2C_BUS_E bus);
MPRL_Response_S MPRL_ReadData(I2C_BUS_E bus);
