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
#include "app_vehicleState.h"
#include "drv_vn9008.h"
#include "drv_tps2hb16ab.h"
#include "powerManager.h"

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
#define set_vehicleState(m,b,n,s) set(m,b,n,s, app_vehicleState_getStateCAN())
#define set_pumpHsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_vn9008_getState(DRV_VN9008_CHANNEL_PUMP)))
#define set_fanHsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_vn9008_getState(DRV_VN9008_CHANNEL_FAN)))
#define set_bms1HsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_BMS1_SHUTDOWN, DRV_TPS2HB16AB_OUT_1)))
#define set_shutdownHsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_BMS1_SHUTDOWN, DRV_TPS2HB16AB_OUT_2)))
#define set_bms2HsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_BMS2_ACCUM, DRV_TPS2HB16AB_OUT_1)))
#define set_accumHsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_BMS2_ACCUM, DRV_TPS2HB16AB_OUT_2)))
#define set_bms3HsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_BMS3_SENSOR, DRV_TPS2HB16AB_OUT_1)))
#define set_sensorHsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_BMS3_SENSOR, DRV_TPS2HB16AB_OUT_2)))
#define set_vc1HsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_VC1_VC2, DRV_TPS2HB16AB_OUT_1)))
#define set_vc2HsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_VC1_VC2, DRV_TPS2HB16AB_OUT_2)))
#define set_mcHsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_MC_VCU3, DRV_TPS2HB16AB_OUT_1)))
#define set_vcu3HsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_MC_VCU3, DRV_TPS2HB16AB_OUT_2)))
#define set_hveHsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_HVE_COCKPIT, DRV_TPS2HB16AB_OUT_1)))
#define set_cockpitHsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_HVE_COCKPIT, DRV_TPS2HB16AB_OUT_2)))
#define set_spareHsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_SPARE_BMS4, DRV_TPS2HB16AB_OUT_1)))
#define set_bms4HsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_SPARE_BMS4, DRV_TPS2HB16AB_OUT_2)))
#define set_vcu1HsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_VCU1_VCU2, DRV_TPS2HB16AB_OUT_1)))
#define set_vcu2HsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_VCU1_VCU2, DRV_TPS2HB16AB_OUT_2)))
#define set_bms5HsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_BMS5_BMS6, DRV_TPS2HB16AB_OUT_1)))
#define set_bms6HsdState(m,b,n,s) set(m,b,n,s, drv_hsd_getCANState(drv_tps2hb16ab_getState(DRV_TPS2HB16AB_IC_BMS5_BMS6, DRV_TPS2HB16AB_OUT_2)))
#define set_glvVoltage(m,b,n,s) set(m,b,n,s, powerManager_getGLVVoltage())

#include "TemporaryStubbing.h"
