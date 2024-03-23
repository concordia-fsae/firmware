/**
 * @file CELL.h
 * @brief  Header file for cell library
 *
 * @note    This header file defines the methods that a compiled source file must provide
 *          for the BMS to properly utilize the data. Only one source file can be compiled
 *          per binary reducing the size of the system.
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stdint.h"


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

uint8_t CELL_getSoCfromV(uint16_t tenth_mv);
uint16_t CELL_getVfromCapacity(uint32_t tenth_mah);
