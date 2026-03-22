/**
 * @file HW_SHT40.h
 * @brief  Header file for SHT40 Driver
 *
 * @note Not used in Release A.1
 */

# pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
# include "stdbool.h"
# include "stdint.h"

// Firmware Includes
# include "HW_i2c.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    SHT40_INIT = 0x00,
    SHT40_WAITING,
    SHT40_MEASURING,
    SHT40_HEATING,
    SHT40_ERROR,
} drv_sht40_state_E;

typedef enum
{
    SHT40_HEAT_LOW = 0x00,
    SHT40_HEAT_MED,
    SHT40_HEAT_HIGH,
} drv_sht40_heat_E;

typedef struct
{
    uint64_t  raw;
    float32_t temp; // [deg C], precision 0.01 deg C
    float32_t rh;   // [%], precision 0.01%
} drv_sht40_data_S;

typedef struct
{
    drv_sht40_state_E      state;
    HW_I2C_Device_S* dev;
    uint32_t         serial_number;
    drv_sht40_data_S       data;
} drv_sht40_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool drv_sht40_init(drv_sht40_S * sht_chip);
bool drv_sht40_startConversion(drv_sht40_S * sht_chip);
bool drv_sht40_getData(drv_sht40_S * sht_chip);
bool drv_sht40_startHeater(drv_sht40_S * sht_chip, drv_sht40_heat_E heat);
