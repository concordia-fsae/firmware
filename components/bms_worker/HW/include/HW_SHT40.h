/**
 * @file HW_SHT40.h
 * @brief  Header file for SHT40 Driver
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2024-01-19
 */

#if defined (BMSW_BOARD_VA3)

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_i2c.h"
#include "stdbool.h"
#include "stdint.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

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
    uint64_t raw;
    int16_t temp; /**< Stored in 0.1 deg C */
    uint16_t rh; /**< Stored in 0.01% RH */
} SHT40_Data_S;

typedef struct{
    HW_I2C_Device_S* dev;
    uint32_t serial_number;
    SHT40_Data_S data;
} SHT40_S;

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool SHT40_Init(void);
bool SHT40_StartConversion(void);
bool SHT40_GetData(void);
bool SHT40_StartHeater(void);

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

#endif /**< BMSW_BOARD_VA3 */
