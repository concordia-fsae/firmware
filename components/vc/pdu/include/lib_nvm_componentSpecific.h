/**
 * lib_nvm_config.h
 * lib_nvm Component Specific file
 */

#pragma once

#include "HW_tim.h"
#include "HW_flash.h"
#include "lib_nvm.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define LIB_NVM_GET_TIME_MS HW_TIM_getTimeMS
#define LIB_NVM_GET_FLASH_PAGE_SIZE FLASH_getPageSize
#define LIB_NVM_CLEAR_FLASH_PAGES FLASH_erasePages
#define LIB_NVM_WRITE_TO_FLASH(addr, data, bytes) FLASH_writeHalfwords(addr, data, bytes / sizeof(storage_t))

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    NVM_ENTRYID_LOG = 0U,
    NVM_ENTRYID_CYCLE,
    NVM_ENTRYID_IMU_CALIB,
    NVM_ENTRYID_COUNT, // All entries must be added to the end!
} lib_nvm_entryId_E;
