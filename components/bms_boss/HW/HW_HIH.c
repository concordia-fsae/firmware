/**
 * @file HW_HIH.c
 * @brief  Source code for HIH driver
 */

#include "HW_HIH.h"

#include <string.h>


extern HW_I2C_Handle_T i2c1;

HW_I2C_Device_S I2C_HIH = {
    .addr   =  0x27,
    .handle = &i2c,
};

HIH_S hih_chip = {
    .dev = &I2C_HIH,
};

bool HIH_init(void)
{
    memset(&hih_chip.data, 0x00, sizeof(hih_chip.data));
    
    if (!HW_I2C_masterWrite(hih_chip.dev, 0x00, 0, 10))
    {
        hih_chip.data.state = HIH_ERROR;
        return false;
    }

    hih_chip.data.state = HIH_MEASURING;

    return true;
}

bool HIH_getData(void)
{
    uint8_t rdat[4] = {0x00};
    if (hih_chip.data.state == HIH_MEASURING)
    {
        if (HW_I2C_masterRead(hih_chip.dev, (uint8_t*)&rdat, 4, 10))
        {
            hih_chip.data.rh = ((rdat[0] & 0x3f) << 8) | rdat[1];
            hih_chip.data.temp = (rdat[2] << 6) | (rdat[3] >> 2);
            hih_chip.data.state = HIH_WAITING;
            return true;
        }
    }
    return false;
}

bool HIH_startConversion(void)
{
    if (hih_chip.data.state == HIH_WAITING)
    {
        if (HW_I2C_masterWrite(hih_chip.dev, 0x00, 0, 10))
        {
            hih_chip.data.state = HIH_MEASURING;
            return true;
        }
    }

    return false;
}
