#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    NVM_ENTRYID_LOG,
    NVM_ENTRYID_CYCLE,
    NVM_ENTRYID_VALUE,
    NVM_ENTRYID_OTHER,
    NVM_ENTRYID_COUNT,
} lib_nvm_entryId_E;

uint32_t HW_TIM_getTimeMS(void);
bool     HW_mcuShuttingDown(void);
uint32_t test_page_size(void);
bool     test_erase(uint32_t address, uint16_t pages);
bool     test_program(uint32_t address, const void *source, uint32_t bytes);
#define LIB_NVM_GET_TIME_MS            HW_TIM_getTimeMS
#define LIB_NVM_GET_FLASH_PAGE_SIZE    test_page_size
#define LIB_NVM_CLEAR_FLASH_PAGES      test_erase
#define LIB_NVM_WRITE_TO_FLASH         test_program
