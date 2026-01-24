/**
 * @file NVM.c
 * @brief  Source code for Non Volatile Memory Manager
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_nvm.h"
#include "app_vehicleSpeed.h"
#include "steeringAngle.h"
#include <string.h>

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern lib_nvm_nvmRecordLog_S recordLog;
extern lib_nvm_nvmCycleLog_S cycleLog;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
static const nvm_odometer_S odometer_data_default = {
    .km = 0.0f,
    .spare = { 0U },
};
static const nvm_steeringCalibration_S steerinCalibration_data_default = {
    .zero = 0.0f,
    .spare = { 0U },
};
LIB_NVM_MEMORY_REGION(nvm_odometer_S odometer_data) = { 0U };

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
    [NVM_ENTRYID_ODOMETER] = {
        .entrySize = sizeof(nvm_odometer_S),
        .entryDefault_Ptr = &odometer_data_default,
        .entryRam_Ptr = &odometer_data,
        .minTimeBetweenWritesMs = 10000U,
        .version = 0U,
    },
    [NVM_ENTRYID_STEERINGCALIBRATION] = {
        .entrySize = sizeof(nvm_steeringCalibration_S),
        .entryDefault_Ptr = &steerinCalibration_data_default,
        .entryRam_Ptr = &steeringCalibration_data,
        .minTimeBetweenWritesMs = 60000U,
        .version = 0U,
    },
};
#endif
