/*
 * HW_CRC.h
 * Header file for the hardware CRC peripheral implementation
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Types.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CRC_BASE        (0x40023000UL)      // base address of the CRC peripheral

#define CRC_DR          (CRC_BASE + 0x00UL) // Data register address
#define CRC_CR          (CRC_BASE + 0x08UL) // Control register address

#define CRC_CR_RESET    (0x01UL) // reset the data register to all Fs


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

uint32_t CRC_mpeg2Calculate(uint32_t *data, uint16_t dataLen);
void     CRC_mpeg2Reset(void);
void     CRC_init(void);
