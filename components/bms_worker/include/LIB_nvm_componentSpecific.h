/**
 * LIB_nvm_config.h
 * LIB_nvm Component Specific file
 */

#pragma once

#include "HW_tim.h"
#include "HW_flash.h"
#include "LIB_nvm.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

#define LIB_NVM_GET_TIME_MS HW_TIM_getTimeMS
#define LIB_NVM_GET_FLASH_PAGE_SIZE FLASH_getPageSize
#define LIB_NVM_CLEAR_FLASH_PAGES FLASH_erasePages
#define LIB_NVM_WRITE_TO_FLASH(addr, data, bytes) FLASH_writeHalfwords(addr, data, bytes / sizeof(storage_t))

typedef enum
{
    NVM_ENTRYID_COUNT = 0U,
} lib_nvm_entryId_E;
