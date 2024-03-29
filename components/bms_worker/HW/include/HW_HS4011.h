/**
 * @file HW_HS4011.h
 * @brief  Header file for HS4011 Relative Humidity/Temperature sensor
 *
 * @note This File is only for Release A.1. Subsequent versions replaced this device.
 */

#pragma once

#if defined(BMSW_BOARD_VA1)

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
# include "stdbool.h"
# include "stdint.h"

// Firmware Includes
# include "HW_i2c.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

# define HS4011_HIGH_PRECISION 0
# define HS4011_MED_PRECISION  1
# define HS4011_LOW_PRECISION  2


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    HS4011_HIGH_PRECISION = 0x00,
    HS4011_MED_PRECISION,
    HS4011_LOW_PRECISION,
} HS4011_Precision_E;

typedef struct
{
    uint8_t precision;
} HS4011_Config_S;

typedef struct
{
    bool     measuring;
    uint32_t raw;
    int16_t  temp; /**< Stored in 0.1 deg C */
    uint16_t rh;   /**< Stored in 0.01% RH */
} HS4011_Data_S;

typedef struct
{
    HW_I2C_Device_S* dev;
    uint32_t         serial_number;
    HS4011_Config_S  config;
    HS4011_Data_S    data;
} HS4011_S;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

bool HS4011_init(void);
bool HS4011_startConversion(void);
bool HS4011_getData(void);

#endif    // BMSW_BOARD_VA1
