/**
 * @file NVM.c
 * @brief  Source code for Non Volatile Memory Manager
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_nvm.h"
#include "BMS.h"
#include <string.h>

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern lib_nvm_nvmRecordLog_S recordLog;
extern lib_nvm_nvmCycleLog_S cycleLog;
extern nvm_bmsbContactorData_S contactor_data;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
static uint16_t version_handler_current(const uint16_t version, const storage_t* const entry_Ptr)
{
    UNUSED(entry_Ptr);
    uint16_t new_version = version;

    if (new_version == 0U)
    {
        // Example: NVM uprec
        nvm_bmsData_S flash;
        memcpy(&flash, entry_Ptr, sizeof(flash.soc));
        current_data.soc = flash.soc;
        new_version = 1;
    }

    return new_version;
}
#endif

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
static const nvm_bmsData_S current_data_default = {
    .soc = 0U,
    .spare = { 0U },
};
LIB_NVM_MEMORY_REGION(nvm_bmsData_S current_data) = { 0U };

static const nvm_bmsbContactorData_S contactor_data_default = {
    .contactorLifetime.contactorHvp = 0U,
    .contactorLifetime.contactorHvn = 0U,
    .contactorLifetime.precharge = 0U,
};
LIB_NVM_MEMORY_REGION(nvm_bmsbContactorData_S contactor_data) = {0U};

const lib_nvm_entry_S lib_nvm_entries[NVM_ENTRYID_COUNT] = {
    [NVM_ENTRYID_LOG] = {
        .entrySize = sizeof(lib_nvm_nvmRecordLog_S),
        .entryDefault_Ptr = &recordLogDefault,
        .entryRam_Ptr = &recordLog,
        .minTimeBetweenWritesMs = 10000U,
        .version = 0U,
    },
    [NVM_ENTRYID_CYCLE] = {
        .entrySize = sizeof(lib_nvm_nvmCycleLog_S),
        .entryDefault_Ptr = &cycleLogDefault,
        .entryRam_Ptr = &cycleLog,
        .minTimeBetweenWritesMs = 60000U, // Should only change once per boot cycle
        .version = 0U,
    },
    [NVM_ENTRYID_CONTACTOR_LIFETIME] = {
        .entrySize = sizeof(nvm_bmsbContactorData_S),
        .entryDefault_Ptr = &contactor_data_default,
        .entryRam_Ptr = &contactor_data,
        .minTimeBetweenWritesMs = 60000U,
        .version = 0U,
    },
    [NVM_ENTRYID_SOC] = {
        .entrySize = sizeof(nvm_bmsData_S),
        .entryDefault_Ptr = &current_data_default,
        .entryRam_Ptr = &current_data,
        .minTimeBetweenWritesMs = 10000U,
        .version = 0U,
        .versionHandler_Fn = &version_handler_current,
    },
};
#endif
