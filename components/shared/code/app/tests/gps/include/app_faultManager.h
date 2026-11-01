#pragma once
#include <stdbool.h>
typedef enum
{
    FM_FAULT_VCFRONT_GPSDEVICEERROR,
    FM_FAULT_VCFRONT_GPSOVERRUN,
    FM_FAULT_VCFRONT_GPSDATANOTVALID,
    TEST_FAULT_COUNT,
} test_fault_E;
void app_faultManager_setFaultState(test_fault_E fault, bool active);
