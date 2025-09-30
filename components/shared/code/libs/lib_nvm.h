/**
 * lib_nvm.h
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
#include "lib_nvm_componentSpecific.h"
#include "Module.h"
#if FEATURE_IS_ENABLED(NVM_SWI)
#include "FreeRTOS_SWI.h"
#endif

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
 *                              E X T E R N S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(NVM_SWI)
extern RTOS_swiHandle_T *NVM_swi;
#endif

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

typedef enum
{
    NVM_ACTION_WRITE,
} lib_nvm_entryAction_E;

typedef struct
{
    const uint16_t version;
    const uint16_t entrySize;
    const void * const entryDefault_Ptr;
    void * const entryRam_Ptr;
    const uint16_t minTimeBetweenWritesMs;
    bool (*versionHandler_Fn)(const uint16_t version, const storage_t* const entry_Ptr);
} lib_nvm_entry_S;

typedef struct
{
    const lib_nvm_entryId_E entryId;
    const lib_nvm_entryAction_E action;
} lib_nvm_entryAction_S;

typedef struct
{
    uint32_t totalRecordWrites;
    uint32_t totalFailedCrc;
    uint32_t totalBlockClears;
    uint32_t totalFailedRecordInit;
    uint32_t totalEmptyRecordInit;
    uint32_t totalRecordsVersionFailed;
    uint32_t spare[1];
} LIB_NVM_STORAGE(lib_nvm_nvmRecordLog_S);

typedef struct
{
    uint32_t totalCycles;
} LIB_NVM_STORAGE(lib_nvm_nvmCycleLog_S);

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

static const lib_nvm_nvmRecordLog_S recordLogDefault = {
    .totalRecordWrites = 0U,
    .totalFailedCrc = 0U,
    .totalBlockClears = 0U,
    .totalFailedRecordInit = 0U,
    .totalEmptyRecordInit = 0U,
    .totalRecordsVersionFailed = 0U,
    .spare = { 0U },
};

static const lib_nvm_nvmCycleLog_S cycleLogDefault = {
    .totalCycles = 0U,
};

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void lib_nvm_init(void);
void lib_nvm_check(void);
void lib_nvm_run(void);
void lib_nvm_cleanUp(void);

bool lib_nvm_nvmInitializeNewBlock(void);
bool lib_nvm_requestWrite(lib_nvm_entryId_E entryId);
bool lib_nvm_writesRequired(void);
bool lib_nvm_writeRequired(lib_nvm_entry_t entryId);

uint32_t lib_nvm_getTotalRecordWrites(void);
uint32_t lib_nvm_getTotalFailedCrc(void);
uint32_t lib_nvm_getTotalBlockErases(void);
uint32_t lib_nvm_getTotalCycles(void);
uint32_t lib_nvm_getTotalFailedRecordInit(void);
uint32_t lib_nvm_getTotalEmptyRecordInit(void);
uint32_t lib_nvm_getTotalRecordsVersionFailed(void);

#endif // NVM_LIB_ENABLED
