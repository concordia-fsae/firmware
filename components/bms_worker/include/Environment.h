/**
 * @file Environment.h
 * @brief  Header file for Environment sensors
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "stdbool.h"
#include "stdint.h"

// Firmware Includes
#include "HW_LTC2983.h"

// Other Includes
#include "Module.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

#if defined(BMSW_BOARD_VA3)
typedef enum
{
    BRD1 = 0x00,
    BRD2,
    BRD_COUNT,
} BRDChannels_E;
#endif /**< BMSW_BOARD_VA3 */


typedef enum
{
    CH1 = 0x00,
    CH2,
    CH3,
    CH4,
    CH5,
    CH6,
    CH7,
    CH8,
    CH9,
    CH10,
    CH11,
    CH12,
    CH13,
    CH14,
    CH15,
    CH16,
    CH17,
    CH18,
    CH19,
    CH20,
    CHANNEL_COUNT,
} ThermistorID_E;

typedef enum
{
    ENV_INIT = 0x00,
    ENV_RUNNING,
    ENV_ERROR,
} Environment_State_E;

typedef struct
{
    bool    therm_error;
    int16_t temp;
} Temperature_S;

typedef struct
{
    struct
    {
        int16_t  mcu_temp;            /**< Stored in 0.1 deg C */
        int16_t  brd_temp[BRD_COUNT]; /**< Stored in 0.1 deg C */
        int16_t  ambient_temp;        /**< Stored in 0.1 deg C */
        uint16_t rh;                  /**< Stored in 0.01% RH */
    } board;
    struct
    {
        Temperature_S temps[CHANNEL_COUNT]; /**< Stored in 0.1 deg C */
        int16_t       max_temp;
        int16_t       min_temp;
        int16_t       avg_temp;
    } cells;
} Env_Variables_S;

typedef struct
{
    Environment_State_E state;
    Env_Variables_S     values;
} Environment_S;


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern Environment_S ENV;
