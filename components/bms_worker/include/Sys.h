/**
 * @file Sys.h
 * @brief  Header file for System Manager
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Firmware Includes */
#include "HW_gpio.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef uint8_t BMS_errors_t;

typedef enum
{
    SYS_ERROR_BMS = 0x00,
    SYS_ERROR_CELL,
    SYS_ERROR_RH,
    SYS_ERROR_AMBIENT_TEMP,
    SYS_ERROR_BOARD_TEMP,
    SYS_ERROR_CNT,
} SYS_errors_E;

_Static_assert(sizeof(BMS_errors_t) > SYS_ERROR_CNT / 8, "Not enough bits in Sys_S.errors to store all errors.");

typedef enum
{
    SYS_INIT = 0x00,
    SYS_RUNNING,
    SYS_ERROR,
} SYS_state_E;

typedef struct
{
    SYS_state_E  state;
    BMS_errors_t errors;
} SYS_S;
