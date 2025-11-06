/**
 RX_config* CAN.h
 * Header file for CANRX configuration
 */

#pragma once


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// imports for timebase
#include "HW_tim.h"

// imports for CAN generated types
#include "CANTypes_generated.h"

// imports for data access
#include "Module.h"
#include "drv_inputAD.h"
#include "drv_userInput.h"
#include "driverInput.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define BTN_LEFT_TOP       USERINPUT_BUTTON_LEFT_TOP
#define BTN_LEFT_MID       USERINPUT_BUTTON_LEFT_MID
#define BTN_LEFT_BOT       USERINPUT_BUTTON_LEFT_BOT
#define BTN_RIGHT_TOP      USERINPUT_BUTTON_RIGHT_TOP
#define BTN_RIGHT_MID      USERINPUT_BUTTON_RIGHT_MID
#define BTN_RIGHT_BOT      USERINPUT_BUTTON_RIGHT_BOT
#define BTN_LEFT_TOGGLE    USERINPUT_BUTTON_LEFT_TOGGLE
#define BTN_RIGHT_TOGGLE   USERINPUT_BUTTON_RIGHT_TOGGLE

#define CANIO_UDS_BUFFER_LENGTH 8U
#define CANIO_getTimeMs() (HW_TIM_getTimeMS())

#define set_taskUsage1kHz(m,b,n,s) set(m,b,n,s, Module_getTotalRuntimePercentage(MODULE_1kHz_TASK));
#define set_taskUsage100Hz(m,b,n,s) set(m,b,n,s, Module_getTotalRuntimePercentage(MODULE_100Hz_TASK));
#define set_taskUsage10Hz(m,b,n,s) set(m,b,n,s, Module_getTotalRuntimePercentage(MODULE_10Hz_TASK));
#define set_taskUsage1Hz(m,b,n,s) set(m,b,n,s, Module_getTotalRuntimePercentage(MODULE_1Hz_TASK));
#define set_taskUsageIdle(m,b,n,s) set(m,b,n,s, Module_getTotalRuntimePercentage(MODULE_IDLE_TASK));

#define set_leftTop(m,b,n,s)             set(m,b,n,s, drv_userInput_buttonPressed(BTN_LEFT_TOP)        ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_leftMid(m,b,n,s)             set(m,b,n,s, drv_userInput_buttonPressed(BTN_LEFT_MID)        ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_leftBot(m,b,n,s)             set(m,b,n,s, drv_userInput_buttonPressed(BTN_LEFT_BOT)        ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_rightTop(m,b,n,s)            set(m,b,n,s, drv_userInput_buttonPressed(BTN_RIGHT_TOP)       ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_rightMid(m,b,n,s)            set(m,b,n,s, drv_userInput_buttonPressed(BTN_RIGHT_MID)       ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_rightBot(m,b,n,s)            set(m,b,n,s, drv_userInput_buttonPressed(BTN_RIGHT_BOT)       ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_leftToggle(m,b,n,s)          set(m,b,n,s, drv_userInput_buttonPressed(BTN_LEFT_TOGGLE)     ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_rightToggle(m,b,n,s)         set(m,b,n,s, drv_userInput_buttonPressed(BTN_RIGHT_TOGGLE)    ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_leftTopDebounce(m,b,n,s)     set(m,b,n,s, drv_userInput_buttonInDebounce(BTN_LEFT_TOP)     ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_leftMidDebounce(m,b,n,s)     set(m,b,n,s, drv_userInput_buttonInDebounce(BTN_LEFT_MID)     ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_leftBotDebounce(m,b,n,s)     set(m,b,n,s, drv_userInput_buttonInDebounce(BTN_LEFT_BOT)     ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_rightTopDebounce(m,b,n,s)    set(m,b,n,s, drv_userInput_buttonInDebounce(BTN_RIGHT_TOP)    ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_rightMidDebounce(m,b,n,s)    set(m,b,n,s, drv_userInput_buttonInDebounce(BTN_RIGHT_MID)    ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_rightBotDebounce(m,b,n,s)    set(m,b,n,s, drv_userInput_buttonInDebounce(BTN_RIGHT_BOT)    ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_leftToggleDebounce(m,b,n,s)  set(m,b,n,s, drv_userInput_buttonInDebounce(BTN_LEFT_TOGGLE)  ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_rightToggleDebounce(m,b,n,s) set(m,b,n,s, drv_userInput_buttonInDebounce(BTN_RIGHT_TOGGLE) ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)

#define set_requestRun(m,b,n,s)        set(m,b,n,s, driverInput_getDigital(DRIVERINPUT_REQUEST_RUN)        ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_requestReverse(m,b,n,s)    set(m,b,n,s, driverInput_getDigital(DRIVERINPUT_REQUEST_REVERSE)    ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_requestRaceMode(m,b,n,s)   set(m,b,n,s, driverInput_getDigital(DRIVERINPUT_REQUEST_RACE)       ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_requestTorqueInc(m,b,n,s)  set(m,b,n,s, driverInput_getDigital(DRIVERINPUT_REQUEST_TORQUE_INC) ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_requestTorqueDec(m,b,n,s)  set(m,b,n,s, driverInput_getDigital(DRIVERINPUT_REQUEST_TORQUE_DEC) ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)

#define set_requestTractionControl(m,b,n,s) set(m,b,n,s, driverInput_getDigital(DRIVERINPUT_REQUEST_TC) ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)

#include "TemporaryStubbing.h"
