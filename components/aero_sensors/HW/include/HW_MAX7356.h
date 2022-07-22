/**
 * @file HW_MAX7356.h
 * @brief  Header file for MAX7356EUG+ driver
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-21
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include <stdint.h>


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define MAX_GATE(x)     0x01 << x
#define MAX_ALL_GATES   0xff
#define MAX_BUS_TIMEOUT 5


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void     MAX_SetGates(uint8_t command);
uint16_t MAX_ReadGates(void);
