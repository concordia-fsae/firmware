/**
 * @file crashSensor.h
 * @brief  Crash sensor task interface for VCPDU
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_nvm.h"
#include "LIB_Types.h"
#include "CANTypes_generated.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    CRASHSENSOR_INIT = 0x00U,
    CRASHSENSOR_OK,
    CRASHSENSOR_CRASHED,
    CRASHSENSOR_ERROR,
} crashSensor_state_E;

typedef struct
{
    bool crashLatched;
    uint8_t reserved[16U];
} LIB_NVM_STORAGE(nvm_crashState_S);
extern nvm_crashState_S crashState_data;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void crashSensor_task(void);
crashSensor_state_E crashSensor_getState(void);
CAN_crashSensorState_E crashSensor_getStateCAN(void);
float32_t crashSensor_getTrippedAcceleration(void);
float32_t crashSensor_getMaxAcceleration(void);

void crashSensor_notifyFromImu(void);
