#pragma once

#include "LIB_Types.h"

#define LIB_NVM_STORAGE(x)    x
#define NVM_SIZE_ASSERT(entry, size)

typedef enum
{
    NVM_ENTRYID_LOG = 0U,
    NVM_ENTRYID_CYCLE,
    NVM_ENTRYID_ODOMETER,
    NVM_ENTRYID_STEERINGCALIBRATION,
    NVM_ENTRYID_TC_PARAMSTATE,
    NVM_ENTRYID_TC_PID,
    NVM_ENTRYID_COUNT,
} lib_nvm_entryId_E;

bool lib_nvm_requestWrite(lib_nvm_entryId_E entryId);
