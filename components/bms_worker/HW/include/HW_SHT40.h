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
} SHT40_State_E;

typedef struct
{
    SHT40_State_E state;
    uint64_t      raw;
    int16_t       temp; /**< Stored in 0.1 deg C */
    uint16_t      rh;   /**< Stored in 0.01% RH */
} SHT40_Data_S;

typedef struct
{
    HW_I2C_Device_S* dev;
    uint32_t         serial_number;
    SHT40_Data_S     data;
} SHT40_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool SHT40_Init(void);
bool SHT40_StartConversion(void);
bool SHT40_GetData(void);
bool SHT40_StartHeater(void);

#endif    // BMSW_BOARD_VA3
