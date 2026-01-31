/**
 * @file NVM.c
 * @brief  Source code for Non Volatile Memory Manager
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_nvm.h"
#include "crashSensor.h"
#include "drv_imu.h"
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
const nvm_imuCalibration_S imuCalibration_default = {
    .zeroAccel = { { 0.0f } },
    .zeroGyro  = { { 0.0f } },
    .rotation  = {
        {
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
        }
    },
};
LIB_NVM_MEMORY_REGION(nvm_imuCalibration_S imuCalibration_data) = { 0U };
static const nvm_crashState_S crashState_default = {
    .crashLatched = true,
    .reserved = { 0U },
};
LIB_NVM_MEMORY_REGION(nvm_crashState_S crashState_data) = { 0U };

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
    [NVM_ENTRYID_IMU_CALIB] = {
        .entrySize = sizeof(nvm_imuCalibration_S),
        .entryDefault_Ptr = &imuCalibration_default,
        .entryRam_Ptr = &imuCalibration_data,
        .minTimeBetweenWritesMs = 60000U,
        .version = 0U,
    },
    [NVM_ENTRYID_CRASH_STATE] = {
        .entrySize = sizeof(nvm_crashState_S),
        .entryDefault_Ptr = &crashState_default,
        .entryRam_Ptr = &crashState_data,
        .minTimeBetweenWritesMs = 0U,
        .version = 0U,
    },
};
#endif
