/**
 * @file IMD.c
 * @brief  Source code for IMD manager
 */

#include "IMD.h"

#include <string.h>
#include "lib_utility.h"
#include "HW.h"
#include "BMS.h"

#define IMD_OHMS_PER_VOLT 500

#ifndef IMD_MEASUREMENT_TIMEOUT
# define IMD_MEASUREMENT_TIMEOUT 500    // ms
#endif                                  // ifndef IMD_MEASUREMENT_TIMEOUT

#ifndef IMD_ISOLATION_MIN
# define IMD_ISOLATION_MIN ((BMS_CONFIGURED_PACK_MAX_VOLTAGE * IMD_OHMS_PER_VOLT) / 1000)
#endif    // ifndef IMD_ISOLATION_MIN


typedef struct
{
    uint32_t    last_measurement;
    float32_t   isolation;    // [kOhm], Precision: 1kOhm
    IMD_SST_E   sst_state;
    IMD_State_E state;
    bool        fault: 1;
} imd_S;

imd_S imd;

void IMD_init(void)
{
    memset(&imd, 0x00, sizeof(imd));
    imd.fault     = true;
    imd.state     = IMD_ERROR;
    imd.sst_state = IMD_SST_BAD;
}

void IMD_setMlsMeasurement(uint32_t freq, uint32_t duty)
{
    float32_t dutyf = (float32_t)duty;
    float32_t kOhm = 0;

    SATURATE(5.1f, dutyf, 94.9);

    if (freq < 25) {
        kOhm = (0.9f * 1200) / ((dutyf / 100) - 0.05f) - 1200.0f;
        IMD_setFault(false);
        IMD_setSST(true);
        imd.isolation        = kOhm;
        imd.last_measurement = HW_getTick();
    }
    else if ((freq >= 25) && (freq <= 35))
    {
        if (duty < 15) {
            IMD_setSST(true);
        }
        else
        {
            IMD_setSST(false);
        }
    }
    else if (freq > 35)
    {
        IMD_setFault(true);
    }

    if (kOhm > IMD_ISOLATION_MIN)
    {
        imd.state = IMD_HEALTHY;
        imd.fault = false;
    }
    else
    {
        imd.state = IMD_UNHEALTHY;
        imd.fault = true;
    }
}

void IMD_setSST(bool good)
{
    if (good)
    {
        imd.sst_state = IMD_SST_GOOD;
    }
    else
    {
        imd.sst_state = IMD_SST_BAD;
        imd.fault     = true;
    }
}

void IMD_setFault(bool fault)
{
    if (fault)
    {
        imd.isolation = 0;
        imd.state     = IMD_FAULT;
        imd.fault     = true;
    }
}

bool IMD_timeout(void)
{
    if (imd.last_measurement < (HW_getTick() - IMD_MEASUREMENT_TIMEOUT))
    {
        imd.fault     = true;
        imd.state     = IMD_ERROR;
        imd.isolation = 0;
        return true;
    }

    return false;
}
IMD_State_E IMD_getState(void)
{
    (void)IMD_timeout();
    return imd.state;
}

IMD_SST_E IMD_getSST(void)
{
    return imd.sst_state;
}

float32_t IMD_getIsolation(void)
{
    (void)IMD_timeout();
    return imd.isolation;
}
