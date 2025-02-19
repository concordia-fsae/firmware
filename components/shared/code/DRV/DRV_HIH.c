/**
 * @file HW_HIH.c
 * @brief  Source code for HIH driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "DRV_HIH.h"
#include <string.h>

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern HW_I2C_Device_S I2C_HIH;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

bool DRV_HIH_init(void)
{
    memset(&hih_chip.data, 0x00, sizeof(hih_chip.data));

    if (!HW_I2C_masterWrite(hih_chip.dev, 0x00, 0, 10))
    {
        hih_chip.data.state = DRV_HIH_ERROR;
        return false;
    }

    hih_chip.data.state = DRV_HIH_MEASURING;

    return true;
}

bool DRV_HIH_getData(void)
{
    uint8_t rdat[4] = {0x00};
    if (hih_chip.data.state == DRV_HIH_MEASURING)
    {
        if (HW_I2C_masterRead(hih_chip.dev, (uint8_t*)&rdat, 4, 10))
        {
            hih_chip.data.rh = ((rdat[0] & 0x3f) << 8) | rdat[1];
            hih_chip.data.temp = (rdat[2] << 6) | (rdat[3] >> 2);
            hih_chip.data.state = DRV_HIH_WAITING;
            return true;
        }
    }
    return false;
}

bool DRV_HIH_startConversion(void)
{
    if (hih_chip.data.state == DRV_HIH_WAITING)
    {
        if (HW_I2C_masterWrite(hih_chip.dev, 0x00, 0, 10))
        {
            hih_chip.data.state = DRV_HIH_MEASURING;
            return true;
        }
    }

    return false;
}
