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

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
static bool version_handler_current(const uint16_t version, const storage_t* const entry_Ptr)
{
    UNUSED(entry_Ptr);
    uint16_t new_version = version;

    if (new_version == 0U)
    {
        // nvm_bms_data_S flash;
        // memcpy(&flash, entry_Ptr, sizeof(flash.pack_amp_hours));
        // current_data.pack_amp_hours = flash.pack_amp_hours;
        // new_version = 1;
    }

    return version != new_version;
}
#endif

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
static const nvm_bms_data_S current_data_default = {
    .pack_amp_hours = 0U,
    .spare = { 0U },
};
LIB_NVM_MEMORY_REGION(nvm_bms_data_S current_data) = { 0U };

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
    [NVM_ENTRYID_COULOMB_COUNT] = {
        .entrySize = sizeof(nvm_bms_data_S),
        .entryDefault_Ptr = &current_data_default,
        .entryRam_Ptr = &current_data,
        .minTimeBetweenWritesMs = 10000U,
        .version = 1U,
        .versionHandler_Fn = &version_handler_current,
    },
};
#endif
