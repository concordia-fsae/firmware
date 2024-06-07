/**
 * @file Environment.h
 * @brief  Header file for Environment sensors
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "FloatTypes.h"
#include "stdbool.h"
#include "stdint.h"

// Firmware Includes

// Other Includes
#include "Module.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    ENV_INIT = 0x00,
    ENV_RUNNING,
    ENV_ERROR,
} Environment_State_E;

typedef struct
{
    Environment_State_E state;
    struct
    {
        float32_t mcu_temp;     /**< Stored in 0.1 deg C */
        float32_t ambient_temp; /**< Stored in 0.1 deg C */
        float32_t rh;           /**< Stored in 0.01% RH */
    } board;
} ENV_S;


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern ENV_S ENV;
