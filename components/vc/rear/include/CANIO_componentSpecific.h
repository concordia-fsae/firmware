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
#include "brakeLight.h"
#include "brakePressure.h"
#include "horn.h"
#include "tssi.h"
#include "drv_tps20xx.h"
#include "mcManager.h"
#include "shockpot.h"

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
#define set_brakeLightState(m,b,n,s) set(m,b,n,s, brakeLight_getStateCAN())
#define set_hornState(m,b,n,s) set(m,b,n,s, horn_getStateCAN())
#define set_tssiState(m,b,n,s) set(m,b,n,s, tssi_getStateCAN())
#define set_brakePressure(m,b,n,s) set(m,b,n,s, brakePressure_getBrakePressure())
#define set_5vCriticalHsdState(m,b,n,s) set(m,b,n,s, drv_tps20xx_getStateCAN(DRV_TPS20XX_CHANNEL_5V_CRITICAL))
#define set_5vExtHsdState(m,b,n,s) set(m,b,n,s, drv_tps20xx_getStateCAN(DRV_TPS20XX_CHANNEL_5V_EXT))
#define set_torqueCommand(m,b,n,s) set(m,b,n,s, mcManager_getTorqueCommand())
#define set_directionCommand(m,b,n,s) set(m,b,n,s, mcManager_getDirectionCommand())
#define set_inverterEnable(m,b,n,s) set(m,b,n,s, mcManager_getEnableCommand())
#define set_torqueLimit(m,b,n,s) set(m,b,n,s, mcManager_getTorqueLimit())

// TODO: Improve this interface to support other parameters
#define transmit_VCREAR_mcEepromCommand mcManager_clearEepromCommand()
#define set_eepromAddress(m,b,n,s) set(m,b,n,s, CAN_PM100DXEEPROMADDRESS_FAULT_CLEAR)
#define set_eepromCommand(m,b,n,s) set(m,b,n,s, CAN_PM100DXEEPROMRWCOMMAND_WRITE)
#define set_eepromDataRaw(m,b,n,s) set(m,b,n,s, 0U)

#define set_shockpotdispRL(m,b,n,s) set(m,b,n,s, shockpot_getRLDisp())
#define set_shockpotdispRR(m,b,n,s) set(m,b,n,s, shockpot_getRRDisp())
#define set_shockpotVoltRL(m,b,n,s) set(m,b,n,s, shockpot_getRLVoltage())
#define set_shockpotVoltRR(m,b,n,s) set(m,b,n,s, shockpot_getRRVoltage())

#include "TemporaryStubbing.h"
