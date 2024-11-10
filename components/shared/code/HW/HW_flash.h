/*
 * HW_FLASH.h
 * This file describes low-level, mostly hardware-specific Flash peripheral values
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Types.h"

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void     FLASH_init(void);
void     FLASH_lock(void);
void     FLASH_unlock(void);
uint32_t FLASH_getPageSize(void);
bool     FLASH_erasePages(uint32_t pageAddr, uint16_t pages);
bool     FLASH_writeHalfwords(uint32_t addr, uint16_t *data, uint16_t dataLen);
bool     FLASH_writeWords(uint32_t addr, uint32_t *data, uint16_t dataLen);
