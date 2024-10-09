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
#include "FloatTypes.h"

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t CELL_getSoCfromV(float32_t volt);
