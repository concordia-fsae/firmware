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
#include "FloatTypes.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

#if defined(BMSW_BOARD_VA3)
typedef enum
{
    BRD1 = 0x00,
    BRD2,
    BRD_COUNT,
} ENV_BRDChannels_E;
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
} ENV_thermistorID_E;

typedef enum
{
    ENV_INIT = 0x00,
    ENV_RUNNING,
    ENV_FAULT,
    ENV_ERROR,
} Environment_State_E;

typedef struct
{
    bool    therm_error;
    int16_t temp;
} ENV_temperature_S;

typedef struct
{
    Environment_State_E state;
    struct
    {
        struct
        {
            float32_t  mcu_temp;            /**< Stored in 0.1 deg C */
            float32_t  brd_temp[BRD_COUNT]; /**< Stored in 0.1 deg C */
            float32_t  ambient_temp;        /**< Stored in 0.1 deg C */
            float32_t  rh;                  /**< Stored in 0.01% RH */
        } board;
        ENV_temperature_S temps[CHANNEL_COUNT]; /**< Stored in 0.1 deg C */
        float32_t       max_temp;
        float32_t       min_temp;
        float32_t       avg_temp;
    } values;
} ENV_S;


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern ENV_S ENV;
