/**
 * @file NVM.c
 * @brief  Source code for Non Volatile Memory Manager
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "app_vehicleSpeed.h"
#include "lib_nvm.h"
#include "steeringAngle.h"
#include "torque.h"
#include <string.h>

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern lib_nvm_nvmRecordLog_S recordLog;
extern lib_nvm_nvmCycleLog_S  cycleLog;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
# define TC_PARAMSTATE_NVM_VERSION    1U
# define TC_PID_NVM_VERSION           1U

typedef struct
{
    FLAG_create(params, PARAMSTATE_COUNT);
    uint16_t spare[2U];
} LIB_NVM_STORAGE(nvm_tcParamState_v0_S);

typedef struct
{
    uint8_t  percentMaxTcLimit;
    uint8_t  percentILim;
    uint16_t thousandthKp;
    uint16_t thousandthKi;
    uint16_t thousandthKd;
    uint16_t tLeakMs;
    uint16_t spare[6U];
} LIB_NVM_STORAGE(nvm_tcPid_v0_S);

NVM_SIZE_ASSERT(nvm_tcParamState_v0_S, sizeof(nvm_tcParamState_S));
NVM_SIZE_ASSERT(nvm_tcPid_v0_S,        sizeof(nvm_tcPid_S));

static uint16_t version_handler_tcParamState(const uint16_t version, const storage_t* const entry_Ptr)
{
    uint16_t new_version = version;

    if (new_version == 0U)
    {
        nvm_tcParamState_v0_S flash = { 0U };
        memcpy(&flash,                   entry_Ptr,    sizeof(flash));

        memcpy(tcParamState_data.params, flash.params, sizeof(tcParamState_data.params));
        tcParamState_data.selectedTcMapping = TC_MAPPING_CUSTOM;
        memset(tcParamState_data.spare, 0x00U, sizeof(tcParamState_data.spare));

        new_version                         = TC_PARAMSTATE_NVM_VERSION;
    }

    return new_version;
}

static uint16_t version_handler_tcPid(const uint16_t version, const storage_t* const entry_Ptr)
{
    uint16_t new_version = version;

    if (new_version == 0U)
    {
        nvm_tcPid_v0_S flash = { 0U };
        memcpy(&flash, entry_Ptr, sizeof(flash));

        tcPid_data.percentMaxTcLimit = flash.percentMaxTcLimit;
        tcPid_data.percentILim       = flash.percentILim;
        tcPid_data.thousandthKp      = flash.thousandthKp;
        tcPid_data.thousandthKi      = flash.thousandthKi;
        tcPid_data.thousandthKd      = flash.thousandthKd;
        tcPid_data.tLeakMs           = flash.tLeakMs;
        tcPid_data.maxTorqueNm       = TC_130NM_TORQUE;
        memset(tcPid_data.spare, 0x00U, sizeof(tcPid_data.spare));

        new_version                  = TC_PID_NVM_VERSION;
    }

    return new_version;
}
#endif // if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
static const nvm_odometer_S            odometer_data_default = {
    .km    = 0.0f,
    .spare = { 0U },
};
LIB_NVM_MEMORY_REGION(nvm_odometer_S odometer_data) = { 0U };
static const nvm_steeringCalibration_S steerinCalibration_data_default = {
    .zero  = 0.0f,
    .spare = { 0U },
};
static const nvm_tcParamState_S        tcParamState_data_default = {
    .params            = { 0U },
    .selectedTcMapping = TC_MAPPING_CUSTOM,
    .spare             = { 0U },
};
LIB_NVM_MEMORY_REGION(nvm_tcParamState_S tcParamState_data) = { 0U };
TC_SET_DEFAULT_PID(static const nvm_tcPid_S tcPid_data_default);
LIB_NVM_MEMORY_REGION(nvm_tcPid_S tcPid_data)               = { 0U };

const lib_nvm_entry_S lib_nvm_entries[NVM_ENTRYID_COUNT] = {
    [NVM_ENTRYID_LOG] =                 {
        .entrySize              = sizeof(lib_nvm_nvmRecordLog_S),
        .entryDefault_Ptr       = &recordLogDefault,
        .entryRam_Ptr           = &recordLog,
        .minTimeBetweenWritesMs =                         10000U,
        .version                =                             0U,
    },
    [NVM_ENTRYID_CYCLE] =               {
        .entrySize              = sizeof(lib_nvm_nvmCycleLog_S),
        .entryDefault_Ptr       = &cycleLogDefault,
        .entryRam_Ptr           = &cycleLog,
        .minTimeBetweenWritesMs =                        60000U, // Should only change once per boot cycle
        .version                =                            0U,
    },
    [NVM_ENTRYID_ODOMETER] =            {
        .entrySize              = sizeof(nvm_odometer_S),
        .entryDefault_Ptr       = &odometer_data_default,
        .entryRam_Ptr           = &odometer_data,
        .minTimeBetweenWritesMs =                 10000U,
        .version                =                     0U,
    },
    [NVM_ENTRYID_STEERINGCALIBRATION] = {
        .entrySize              = sizeof(nvm_steeringCalibration_S),
        .entryDefault_Ptr       = &steerinCalibration_data_default,
        .entryRam_Ptr           = &steeringCalibration_data,
        .minTimeBetweenWritesMs =                            60000U,
        .version                =                                0U,
    },
    [NVM_ENTRYID_TC_PARAMSTATE] =       {
        .entrySize              = sizeof(nvm_tcParamState_S),
        .entryDefault_Ptr       = &tcParamState_data_default,
        .entryRam_Ptr           = &tcParamState_data,
        .minTimeBetweenWritesMs =                        10000U,
        .version                = TC_PARAMSTATE_NVM_VERSION,
        .versionHandler_Fn      = &version_handler_tcParamState,
    },
    [NVM_ENTRYID_TC_PID] =              {
        .entrySize              = sizeof(nvm_tcPid_S),
        .entryDefault_Ptr       = &tcPid_data_default,
        .entryRam_Ptr           = &tcPid_data,
        .minTimeBetweenWritesMs =                 10000U,
        .version                = TC_PID_NVM_VERSION,
        .versionHandler_Fn      = &version_handler_tcPid,
    },
};
#endif // if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
