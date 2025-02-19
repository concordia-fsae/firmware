/**
 * @file drv_hih.h
 * @brief  Header file for HIH driver
 * @note This driver supports the HIH6000-HIH8000 devices by honeywell. Presently
 * only the I2C interface is defined.
 *
 * Setup
 * 1. Declare a drv_hih_S with the correct configuration
 * 2. Run the initialization on the chip
 * 3. Periodically start a conversion and subsequently attempt to retrieve the data until ready.
 *    This should be done in a passive manner, only updating when the measurement
 *    has completed
 *
 * Usage
 * - Once a conversion is started (startConversion), the state will enter measuring.
 * - Once a measurement has completed on the chip and getData is called, the
 *   status will report waiting. If getData is called and the chip is not measuring
 *   or there was not a valid transmission by the chip it will return false.
 * - To retrieve the values from the payload call their respective functions.
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_i2c.h"
#include "LIB_Types.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum {
    DRV_HIH_INIT = 0x00,
    DRV_HIH_MEASURING,
    DRV_HIH_WAITING,
    DRV_HIH_ERROR,
} drv_hih_state_E;

typedef struct {
    uint16_t temp;
    uint16_t rh;
    drv_hih_state_E state;
} drv_hih_data_S;

typedef struct {
    HW_I2C_Device_S* dev;
    drv_hih_data_S data;
    float32_t rh;
    float32_t temperature;
} drv_hih_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool            drv_hih_init(drv_hih_S* hih_chip);
bool            drv_hih_getData(drv_hih_S* hih_chip);
bool            drv_hih_startConversion(drv_hih_S* hih_chip);
float32_t       drv_hih_getRH(drv_hih_S* hih_chip);
float32_t       drv_hih_getTemperature(drv_hih_S* hih_chip);
drv_hih_state_E drv_hih_getState(drv_hih_S* hih_chip);

