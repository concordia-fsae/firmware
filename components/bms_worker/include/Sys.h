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

typedef uint8_t BMS_Errors_t;

typedef enum
{
    BMS_ERROR = 0x00,
    CELL_TEMP_ERROR,
    RH_ERROR,
    AMBIENT_TEMP_ERROR,
    BOARD_TEMP_ERROR,
    ERRORS_CNT,
} Sys_Errors_E;

_Static_assert(sizeof(BMS_Errors_t) > ERRORS_CNT / 8, "Not enough bits in Sys_S.errors to store all errors.");

typedef enum
{
    SYS_INIT = 0x00,
    SYS_RUNNING,
    SYS_ERROR,
} Sys_State_E;

typedef struct
{
    Sys_State_E  state;
    BMS_Errors_t errors;
} Sys_S;
