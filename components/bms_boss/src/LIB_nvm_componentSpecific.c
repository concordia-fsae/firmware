/**
 * @file NVM.c
 * @brief  Source code for Non Volatile Memory Manager
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_nvm.h"
#include "IO.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const lib_nvm_entry_S lib_nvm_entries[NVM_ENTRYID_COUNT] = {
    [NVM_ENTRYID_IO_CALIBRATION] = {
        .entryId = NVM_ENTRYID_IO_CALIBRATION,
        .version = IO_CALIBRATIONVALUES_VERSION_0,
        .entrySize = sizeof(IO_calibrationValues_V0_S),
        .entryDefault_Ptr = &IO_calibrationValues_default,
        .entryRam_Ptr = &IO_calibrationValues,
        .minTimeBetweenWritesMs = 60000U,
    },
};
