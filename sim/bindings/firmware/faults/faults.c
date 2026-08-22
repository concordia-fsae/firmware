#include "faults.h"

#include "app_faultManager.h"

bool rig_runtime_get_fault(int fault)
{
    return ((fault >= 0) && (fault < FM_FAULT_COUNT)) ? app_faultManager_getFaultState((FM_fault_E)fault) : false;
}
