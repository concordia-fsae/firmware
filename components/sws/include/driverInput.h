/**
 * @file driverInput.h
 * @brief Handle driver input and control
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stdbool.h"
#include "CANTypes_generated.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRIVERINPUT_REQUEST_RUN = 0x00U,
    DRIVERINPUT_REQUEST_REVERSE,
    DRIVERINPUT_REQUEST_RACE,
    DRIVERINPUT_REQUEST_TORQUE_DEC,
    DRIVERINPUT_REQUEST_TORQUE_INC,
    DRIVERINPUT_REQUEST_TC_SLIP_DEC,
    DRIVERINPUT_REQUEST_TC_SLIP_INC,
    DRIVERINPUT_REQUEST_TC,
    DRIVERINPUT_REQUEST_REGEN,
    DRIVERINPUT_REQUEST_COUNT
} driverInput_inputDigital_E;

typedef enum
{
    DRIVERINPUT_PAGE_HOME = 0x00U,
    DRIVERINPUT_PAGE_BUTTONS,
    DRIVERINPUT_PAGE_DATA1,
    DRIVERINPUT_PAGE_DATA2,
    DRIVERINPUT_PAGE_COUNT,
} driverInput_page_E;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool             driverInput_getDigital(driverInput_inputDigital_E input);
CAN_screenPage_E driverInput_getScreenCAN(void);
