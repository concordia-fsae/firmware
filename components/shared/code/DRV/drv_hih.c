/**
 * @file drv_hih.c
 * @brief  Source code for HIH driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_hih.h"
#include <string.h>

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Initialize the HIH chip
 * @param hih_chip The chip to initialize
 * @return True if the chip successfully initialized, false otherwise
 * @note If there is an error during initialization,
 * the chip will enter the error state
 */
bool drv_hih_init(drv_hih_S* hih_chip)
{
    memset(&hih_chip->data, 0x00, sizeof(hih_chip->data));

    if (!HW_I2C_masterWrite(hih_chip->dev, 0x00, 0, 10))
    {
        hih_chip->data.state = DRV_HIH_ERROR;
        return false;
    }

    hih_chip->data.state = DRV_HIH_MEASURING;

    return true;
}

/**
 * @brief Poll the chip for new data if it is currently measuring.
 * @param hih_chip The chip to request data from
 * @return True if the device has completed a measurement, false otherwise
 * @note This function will also update the internal rh and temperature values
 * in the drv_hih_S of the chip
 */
bool drv_hih_getData(drv_hih_S* hih_chip)
{
    uint8_t rdat[4] = {0x00};
    if (hih_chip->data.state == DRV_HIH_MEASURING)
    {
        if (HW_I2C_masterRead(hih_chip->dev, (uint8_t*)&rdat, 4, 10))
        {
            hih_chip->data.rh = ((rdat[0] & 0x3f) << 8) | rdat[1];
            hih_chip->data.temp = (rdat[2] << 6) | (rdat[3] >> 2);
            hih_chip->data.state = DRV_HIH_WAITING;
            hih_chip->temperature = ((float32_t)hih_chip->data.temp)/16382*165-40;
            hih_chip->rh = ((float32_t)hih_chip->data.rh)/16382*100;
            return true;
        }
    }
    return false;
}

/**
 * @brief Start a conversion
 * @param hih_chip The chip to start a conversion with
 * @return True if the chip started a measurement, false otherwise
 */
bool drv_hih_startConversion(drv_hih_S* hih_chip)
{
    if (hih_chip->data.state == DRV_HIH_WAITING)
    {
        if (HW_I2C_masterWrite(hih_chip->dev, 0x00, 0, 10))
        {
            hih_chip->data.state = DRV_HIH_MEASURING;
            return true;
        }
    }

    return false;
}

/**
 * @brief Get the last measured RH
 * @param hih_chip The chip that took the measurement
 * @return The relative humidity in %
 */
float32_t drv_hih_getRH(drv_hih_S* hih_chip)
{
    return hih_chip->rh;
}

/**
 * @brief Get the last measured temperature
 * @param hih_chip The chip that took the measurement
 * @return The temperature measured in degree celsius
 */
float32_t drv_hih_getTemperature(drv_hih_S* hih_chip)
{
    return hih_chip->temperature;
}

/**
 * @brief Get the last measured temperature
 * @param hih_chip The chip that took the measurement
 * @return The current state of the chip
 */
drv_hih_state_E drv_hih_getState(drv_hih_S* hih_chip)
{
    return hih_chip->data.state;
}
