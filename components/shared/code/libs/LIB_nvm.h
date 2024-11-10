/**
 * LIB_nvm.h
 * Header file for the NVM Manager
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "BuildDefines.h"

#if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
#include "stdbool.h"
#include "stdint.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

// Helper functions for flash memory alignment and for tracking NVM usage
#define LIB_NVM_STORAGE(x) __attribute__((aligned(sizeof(storage_t)))) \
                           (x)
#define LIB_NVM_MEMORY_REGION(x) __attribute__((section(".nvm"))) x##_nvm = { 0U }; \
                                 x
#define LIB_NVM_MEMORY_REGION_ARRAY(x, size) __attribute__((section(".nvm"))) x##_nvm[size] = { 0U }; \
                                             x[size]

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED) && \
    ((MCU_STM32_PN == FDEFS_STM32_PN_STM32F103XB) || \
     (MCU_STM32_PN == FDEFS_STM32_PN_STM32F105))
typedef uint16_t storage_t;
#else
#error "Chipset not supported"
#endif

typedef uint8_t lib_nvm_entry_t;

typedef struct
{
    const lib_nvm_entry_t entryId;
    const uint16_t version;
    const uint8_t entrySize;
    const void * const entryDefault_Ptr;
    void * const entryRam_Ptr;
    const uint16_t minTimeBetweenWritesMs;
} lib_nvm_entry_S;
#include "LIB_nvm_componentSpecific.h"

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void lib_nvm_init(void);
void lib_nvm_run(void);
void lib_nvm_cleanUp(void);

bool lib_nvm_nvmHardReset(void);
bool lib_nvm_nvmHardResetGetStatus(void);
void lib_nvm_requestWrite(lib_nvm_entryId_E entryId);

uint32_t lib_nvm_getTotalRecordWrites(void);
uint32_t lib_nvm_getTotalFailedCrc(void);
uint32_t lib_nvm_getTotalBlockErases(void);
uint32_t lib_nvm_getTotalCycles(void);

#endif // NVM_LIB_ENABLED
