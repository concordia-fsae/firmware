/**
 * @file driverInput.h
 * @brief Handle driver input and control
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stdbool.h"
#include "Yamcan.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRIVERINPUT_REQUEST_NONE = 0x00U,
    DRIVERINPUT_REQUEST_RUN,
    DRIVERINPUT_REQUEST_REVERSE,
    DRIVERINPUT_REQUEST_CRASH_RESET,
    DRIVERINPUT_REQUEST_RACE,
    DRIVERINPUT_REQUEST_TORQUE_DEC,
    DRIVERINPUT_REQUEST_TORQUE_INC,
    DRIVERINPUT_REQUEST_TC_SLIP_DEC,
    DRIVERINPUT_REQUEST_TC_SLIP_INC,
    DRIVERINPUT_REQUEST_TC,
    DRIVERINPUT_REQUEST_REGEN,
    DRIVERINPUT_REQUEST_LAUNCH_CONTROL,
    DRIVERINPUT_REQUEST_PRELOAD_TORQUE_DEC,
    DRIVERINPUT_REQUEST_PRELOAD_TORQUE_INC,
    DRIVERINPUT_REQUEST_CALIBRATE_IMU,
    DRIVERINPUT_REQUEST_CALIBRATE_STEER_ANGLE,
    DRIVERINPUT_REQUEST_APPS_BYPASS,
    DRIVERINPUT_REQUEST_TEST_PUMP,
    DRIVERINPUT_REQUEST_TEST_FAN,
    DRIVERINPUT_REQUEST_OPTION13,
    DRIVERINPUT_REQUEST_TC_TIRE_MODEL_LIM,
    DRIVERINPUT_REQUEST_COUNT,
} driverInput_inputDigital_E;

typedef enum
{
    DRIVERINPUT_PAGE_HOME = 0x00U,
    DRIVERINPUT_PAGE_BUTTONS,
    DRIVERINPUT_PAGE_DATA,
    DRIVERINPUT_PAGE_CONFIG,
    DRIVERINPUT_PAGE_COUNT,
    DRIVERINPUT_PAGE_LAUNCH,
} driverInput_page_E;

typedef enum
{
    DRIVERINPUT_CONFIG_NONE = 0x00U,
    DRIVERINPUT_CONFIG_TC_TIRE_MODEL_LIM,
    DRIVERINPUT_CONFIG_FUNCTION_TEST_PUMPFAN,
    DRIVERINPUT_CONFIG_CALIB_DYNAMICS,
    // Any rules illegal functions shall come after OPTION13
    DRIVERINPUT_CONFIG_OPTION13,
    DRIVERINPUT_CONFIG_VEHICLE_CONTROL,
    DRIVERINPUT_CONFIG_COUNT,
} driverInput_configSelection_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool                  driverInput_getDigital(driverInput_inputDigital_E input);
bool                  driverInput_getOption13(void);
CAN_screenPage_E      driverInput_getScreenCAN(void);
CAN_configSelection_E driverInput_getConfigSelectedCAN(void);
CAN_configOption_E    driverInput_getConfigOptionLeftCAN(void);
CAN_configOption_E    driverInput_getConfigOptionRightCAN(void);
float32_t             driverInput_getConfigValueF32(void);
bool                  driverInput_getConfigHasValueF32(void);
