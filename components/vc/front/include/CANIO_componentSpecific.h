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
#include "drv_pedalMonitor.h"
#include "drv_inputAD.h"
#include "apps.h"
#include "bppc.h"
#include "torque.h"
#include "drv_tempSensors.h"

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CANIO_UDS_BUFFER_LENGTH 8U
#define CANIO_getTimeMs() (HW_TIM_getTimeMS())

#define set_taskUsage1kHz(m,b,n,s) set(m,b,n,s, Module_getTotalRuntimePercentage(MODULE_1kHz_TASK));
#define set_taskUsage100Hz(m,b,n,s) set(m,b,n,s, Module_getTotalRuntimePercentage(MODULE_100Hz_TASK));
#define set_taskUsage10Hz(m,b,n,s) set(m,b,n,s, Module_getTotalRuntimePercentage(MODULE_10Hz_TASK));
#define set_taskUsage1Hz(m,b,n,s) set(m,b,n,s, Module_getTotalRuntimePercentage(MODULE_1Hz_TASK));
#define set_taskUsageIdle(m,b,n,s) set(m,b,n,s, Module_getTotalRuntimePercentage(MODULE_IDLE_TASK));
#define set_apps1(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalPosition(DRV_PEDALMONITOR_APPS1) * 100)
#define set_apps2(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalPosition(DRV_PEDALMONITOR_APPS2) * 100)
#define set_brakePosition(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalPosition(DRV_PEDALMONITOR_BRAKE_POT) * 100)
#define set_acceleratorPosition(m,b,n,s) set(m,b,n,s, apps_getPedalPosition() * 100)
#define set_apps1State(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalStateCAN(DRV_PEDALMONITOR_APPS1))
#define set_apps2State(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalStateCAN(DRV_PEDALMONITOR_APPS2))
#define set_brakeState(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalStateCAN(DRV_PEDALMONITOR_BRAKE_POT))
#define set_apps1Voltage(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalVoltage(DRV_PEDALMONITOR_APPS1))
#define set_apps2Voltage(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalVoltage(DRV_PEDALMONITOR_APPS2))
#define set_brakeVoltage(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalVoltage(DRV_PEDALMONITOR_BRAKE_POT))
#define set_acceleratorState(m,b,n,s) set(m,b,n,s, apps_getStateCAN())
#define set_bppcState(m,b,n,s) set(m,b,n,s, bppc_getStateCAN())
#define set_torqueRequest(m,b,n,s) set(m,b,n,s, torque_getTorqueRequest())
#define set_torqueManagerState(m,b,n,s) set(m,b,n,s, torque_getStateCAN())
#define set_runButtonStatus(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_RUN_BUTTON) == DRV_IO_ACTIVE) ? \
                                                   CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_LBrakeTemp(m,b,n,s) set(m,b,n,s, (drv_tempSensors_getChannelTemperatureDegC(DRV_TEMPSENSOR_L_BR_TEMP)))
//#define set_RBrakeTemp(m,b,n,s) set(m,b,n,s, (0x05))
#define set_RBrakeTemp(m,b,n,s) set(m,b,n,s, (drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_R_BR_TEMP)))
//#define set_RBrakeTemp(m,b,n,s) set(m,b,n,s, (drv_tempSensors_getChannelTemperatureDegC(DRV_TEMPSENSOR_R_BR_TEMP)))

#include "TemporaryStubbing.h"
