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
#include "brakePressure.h"
#include "drv_tps20xx.h"
#include "cockpitLights.h"
#include "steeringAngle.h"
#include "shockpot.h"
#include "Module.h"
#include "wheelSpeed.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(FEATURE_BRAKEPEDAL_FROM_PRESSURE)
#define BRAKE_CHANNEL DRV_PEDALMONITOR_BRAKE_PR
#else
#define BRAKE_CHANNEL DRV_PEDALMONITOR_BRAKE_POT
#endif

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CANIO_UDS_BUFFER_LENGTH 8U
#define CANIO_getTimeMs() (HW_TIM_getTimeMS())

#define set_apps1(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalPosition(DRV_PEDALMONITOR_APPS1) * 100)
#define set_apps2(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalPosition(DRV_PEDALMONITOR_APPS2) * 100)
#define set_brakePosition(m,b,n,s) set(m,b,n,s, bppc_getPedalPosition() * 100)
#define set_acceleratorPosition(m,b,n,s) set(m,b,n,s, apps_getPedalPosition() * 100)
#define set_apps1State(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalStateCAN(DRV_PEDALMONITOR_APPS1))
#define set_apps2State(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalStateCAN(DRV_PEDALMONITOR_APPS2))
#define set_brakeState(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalStateCAN(BRAKE_CHANNEL))
#define set_apps1Voltage(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalVoltage(DRV_PEDALMONITOR_APPS1))
#define set_apps2Voltage(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalVoltage(DRV_PEDALMONITOR_APPS2))
#define set_brakePotVoltage(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalVoltage(DRV_PEDALMONITOR_BRAKE_POT))
#define set_brakePrVoltage(m,b,n,s) set(m,b,n,s, drv_pedalMonitor_getPedalVoltage(DRV_PEDALMONITOR_BRAKE_PR))
#define set_acceleratorState(m,b,n,s) set(m,b,n,s, apps_getStateCAN())
#define set_bppcState(m,b,n,s) set(m,b,n,s, bppc_getStateCAN())
#define set_torqueRequest(m,b,n,s) set(m,b,n,s, torque_getTorqueRequest())
#define set_torqueRequestMaxDebug(m,b,n,s) set(m,b,n,s, torque_getTorqueRequestMax())
#define set_torqueManagerState(m,b,n,s) set(m,b,n,s, torque_getStateCAN())
#define set_runButtonStatus(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_RUN_BUTTON) == DRV_IO_ACTIVE) ? \
                                                   CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_gear(m,b,n,s) set(m,b,n,s, torque_getGearCAN())
#define set_launchControlState(m,b,n,s) set(m,b,n,s, torque_getLaunchControlStateCAN())
#define set_raceMode(m,b,n,s) set(m,b,n,s, torque_getRaceModeCAN())
#define set_brakePressure(m,b,n,s) set(m,b,n,s, brakePressure_getBrakePressure())
#define set_5vCriticalHsdState(m,b,n,s) set(m,b,n,s, drv_tps20xx_getStateCAN(DRV_TPS20XX_CHANNEL_5V_CRITICAL))
#define set_5vExtHsdState(m,b,n,s) set(m,b,n,s, drv_tps20xx_getStateCAN(DRV_TPS20XX_CHANNEL_5V_EXT))
#define set_bmsLightState(m,b,n,s) set(m,b,n,s, cockpitLights_bms_getStateCAN())
#define set_imdLightState(m,b,n,s) set(m,b,n,s, cockpitLights_imd_getStateCAN())
#define set_steeringAngle(m,b,n,s) set (m,b,n,s, steeringAngle_getSteeringAngle())
#define set_steeringAngleVoltage(m,b,n,s) set (m,b,n,s,steeringAngle_getSteeringVoltage())

#define set_shockpotdispFL(m,b,n,s) set(m,b,n,s, shockpot_getFLDisp())
#define set_shockpotdispFR(m,b,n,s) set(m,b,n,s, shockpot_getFRDisp())
#define set_shockpotVoltFL(m,b,n,s) set(m,b,n,s, shockpot_getFLVoltage())
#define set_shockpotVoltFR(m,b,n,s) set(m,b,n,s, shockpot_getFRVoltage())

#define set_wheelSpeedLinearFL(m,b,n,s) set(m,b,n,s, wheelSpeed_getSpeedLinear(WHEEL_FL))
#define set_wheelSpeedLinearFR(m,b,n,s) set(m,b,n,s, wheelSpeed_getSpeedLinear(WHEEL_FR))
#define set_wheelSpeedRotationalFL(m,b,n,s) set(m,b,n,s, wheelSpeed_getSpeedRotational(WHEEL_FL))
#define set_wheelSpeedRotationalFR(m,b,n,s) set(m,b,n,s, wheelSpeed_getSpeedRotational(WHEEL_FR))

#define set_taskStack1kHz(m,b,n,s) set(m,b,n,s, Module_getMinStackLeft(MODULE_1kHz_TASK))
#define set_taskStack100Hz(m,b,n,s) set(m,b,n,s, Module_getMinStackLeft(MODULE_100Hz_TASK))
#define set_taskStack10Hz(m,b,n,s) set(m,b,n,s, Module_getMinStackLeft(MODULE_10Hz_TASK))
#define set_taskStack1Hz(m,b,n,s) set(m,b,n,s, Module_getMinStackLeft(MODULE_1Hz_TASK))

#include "TemporaryStubbing.h"
