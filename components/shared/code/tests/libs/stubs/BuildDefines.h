#pragma once

#include "FeatureDefines_generated.h"

#define FDEFS_STM32_PN_STM32F103XB 2U
#define FDEFS_STM32_PN_STM32F105 3U

#define MCU_STM32_PN FDEFS_STM32_PN_STM32F105
#define NVM_LIB_ENABLED FEATURE_ENABLED
#define NVM_FLASH_BACKED FEATURE_ENABLED
#define NVM_TASK FEATURE_ENABLED
#define NVM_SWI FEATURE_DISABLED
#define NVM_BLOCK_SIZE 256U

void lib_nvm_test_flash_write(uint32_t addr, const void* data, uint16_t len);
void lib_nvm_test_flash_clear(uint32_t addr, uint16_t pages);
uint32_t lib_nvm_test_flash_page_size(void);
uint32_t lib_nvm_test_time_ms(void);
bool HW_mcuShuttingDown(void);

#define LIB_NVM_GET_FLASH_PAGE_SIZE() lib_nvm_test_flash_page_size()
#define LIB_NVM_WRITE_TO_FLASH(addr, data, len) lib_nvm_test_flash_write((addr), (data), (len))
#define LIB_NVM_CLEAR_FLASH_PAGES(addr, pages) lib_nvm_test_flash_clear((addr), (pages))
#define LIB_NVM_GET_TIME_MS() lib_nvm_test_time_ms()
#define HW_TIM_getTimeMS() lib_nvm_test_time_ms()
