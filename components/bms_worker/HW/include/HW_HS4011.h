/**
 * @file HW_HS4011.h
 * @brief  Header file for HS4011 Relative Humidity/Temperature sensor
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version
 * @date 2023-12-19
 */

#pragma once

#if defined(BMSW_BOARD_VA1)

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< System Includes */
# include "HW_i2c.h"
# include "stdbool.h"
# include "stdint.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

# define HS4011_HIGH_PRECISION 0
# define HS4011_MED_PRECISION  1
# define HS4011_LOW_PRECISION  2


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint8_t precision;
} HS4011_Config_S;

typedef struct
{
    bool     measuring;
    uint32_t raw;
    int16_t  temp; /**< Stored in 0.1 deg C */
    uint16_t rh; /**< Stored in 0.01% RH */
} HS4011_Data_S;

typedef struct
{
    HW_I2C_Device_S* dev;
    uint32_t         serial_number;
    HS4011_Config_S  config;
    HS4011_Data_S    data;
} HS4011_S;


/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

bool HS4011_Init(void);
bool HS4011_StartConversion(void);
bool HS4011_GetData(void);


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

#endif /**< BMSW_BOARD_VA1 */
