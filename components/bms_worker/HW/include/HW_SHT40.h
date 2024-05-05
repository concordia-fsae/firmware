/**
 * @file HW_SHT40.h
 * @brief  Header file for SHT40 Driver
 *
 * @note Not used in Release A.1
 */

#if defined(BMSW_BOARD_VA3)

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
    SHT_INIT = 0x00,
    SHT_WAITING,
    SHT_MEASURING,
    SHT_HEATING,
    SHT_ERROR,
} SHT_State_E;

typedef struct
{
    uint64_t  raw;
    float32_t temp; // [deg C], precision 0.01 deg C
    float32_t rh;   // [%], precision 0.01%
} SHT_Data_S;

typedef struct
{
    SHT_State_E      state;
    HW_I2C_Device_S* dev;
    uint32_t         serial_number;
    SHT_Data_S       data;
} SHT_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool SHT_init(void);
bool SHT_startConversion(void);
bool SHT_getData(void);
bool SHT_startHeater(void);

#endif    // BMSW_BOARD_VA3
