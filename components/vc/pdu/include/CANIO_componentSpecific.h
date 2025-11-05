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
#include "Module.h"
#include "NetworkDefines_generated.h"
#include "BuildInfo.h"

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CANIO_UDS_BUFFER_LENGTH 8U
#define CANIO_getTimeMs() (HW_TIM_getTimeMS())

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
#define set_pumpCurrent(m,b,n,s) set(m,b,n,s, drv_vn9008_getCurrent(DRV_VN9008_CHANNEL_PUMP))
#define set_fanCurrent(m,b,n,s) set(m,b,n,s, drv_vn9008_getCurrent(DRV_VN9008_CHANNEL_FAN))
#define set_bms1Current(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_BMS1_SHUTDOWN, DRV_TPS2HB16AB_OUT_1))
#define set_shutdownCurrent(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_BMS1_SHUTDOWN, DRV_TPS2HB16AB_OUT_2))
#define set_bms2Current(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_BMS2_ACCUM, DRV_TPS2HB16AB_OUT_1))
#define set_accumCurrent(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_BMS2_ACCUM, DRV_TPS2HB16AB_OUT_2))
#define set_bms3Current(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_BMS3_SENSOR, DRV_TPS2HB16AB_OUT_1))
#define set_sensorCurrent(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_BMS3_SENSOR, DRV_TPS2HB16AB_OUT_2))
#define set_pduCurrent(m,b,n,s) set(m,b,n,s, powerManager_getPduCurrent())
#define set_pdu5vVoltage(m,b,n,s) set(m,b,n,s, powerManager_getPdu5vVoltage())
#define set_vc1Current(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_VC1_VC2, DRV_TPS2HB16AB_OUT_1))
#define set_vc2Current(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_VC1_VC2, DRV_TPS2HB16AB_OUT_2))
#define set_mcCurrent(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_MC_VCU3, DRV_TPS2HB16AB_OUT_1))
#define set_vcu3Current(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_MC_VCU3, DRV_TPS2HB16AB_OUT_2))
#define set_hveCurrent(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_HVE_COCKPIT, DRV_TPS2HB16AB_OUT_1))
#define set_cockpitCurrent(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_HVE_COCKPIT, DRV_TPS2HB16AB_OUT_2))
#define set_spareCurrent(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_SPARE_BMS4, DRV_TPS2HB16AB_OUT_1))
#define set_bms4Current(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_SPARE_BMS4, DRV_TPS2HB16AB_OUT_2))
#define set_vcu1Current(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_VCU1_VCU2, DRV_TPS2HB16AB_OUT_1))
#define set_vcu2Current(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_VCU1_VCU2, DRV_TPS2HB16AB_OUT_2))
#define set_bms5Current(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_BMS5_BMS6, DRV_TPS2HB16AB_OUT_1))
#define set_bms6Current(m,b,n,s) set(m,b,n,s, drv_tps2hb16ab_getCurrent(DRV_TPS2HB16AB_IC_BMS5_BMS6, DRV_TPS2HB16AB_OUT_2))
#define set_glvVoltage(m,b,n,s) set(m,b,n,s, powerManager_getGLVVoltage())
#define set_glvCurrent(m,b,n,s) set(m,b,n,s, powerManager_getGLVCurrent())
#define set_taskStack1kHz(m,b,n,s) set(m,b,n,s, Module_getMinStackLeft(MODULE_1kHz_TASK))
#define set_taskStack100Hz(m,b,n,s) set(m,b,n,s, Module_getMinStackLeft(MODULE_100Hz_TASK))
#define set_taskStack10Hz(m,b,n,s) set(m,b,n,s, Module_getMinStackLeft(MODULE_10Hz_TASK))
#define set_taskStack1Hz(m,b,n,s) set(m,b,n,s, Module_getMinStackLeft(MODULE_1Hz_TASK))
#define set_runButton(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_RUN_BUTTON) == DRV_IO_ACTIVE) ? \
                                             CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_pduSafetyReset(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_VCU_SFTY_RESET) == DRV_IO_ACTIVE) ? \
                                                 CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_bmsSafetyReset(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_BMS_RESET) == DRV_IO_ACTIVE) ? \
                                                 CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_safetyReset(m,b,n,s) set(m,b,n,s, ((drv_inputAD_getDigitalActiveState(DRV_INPUTAD_VCU_SFTY_RESET) == DRV_IO_ACTIVE) && \
                                               (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_BMS_RESET) == DRV_IO_ACTIVE)) ? \
                                               CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_bmsbSafetyStatus(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_BMS_SAFETY_EN) == DRV_IO_ACTIVE) ? \
                                                    CAN_SHUTDOWNCIRCUITSTATUS_CLOSED: CAN_SHUTDOWNCIRCUITSTATUS_OPEN)
#define set_imdSafetyStatus(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_IMD_SAFETY_EN) == DRV_IO_ACTIVE) ? \
                                                   CAN_SHUTDOWNCIRCUITSTATUS_CLOSED: CAN_SHUTDOWNCIRCUITSTATUS_OPEN)
#define set_bspdSafetyStatus(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_BSPD_MEM) == DRV_IO_ACTIVE) ? \
                                                    CAN_SHUTDOWNCIRCUITSTATUS_CLOSED: CAN_SHUTDOWNCIRCUITSTATUS_OPEN)
#define set_vcSafetyStatus(m,b,n,s) set(m,b,n,s, (drv_outputAD_getDigitalActiveState(DRV_OUTPUTAD_VCU_SFTY_EN) == DRV_IO_ACTIVE) ? \
                                                  CAN_SHUTDOWNCIRCUITSTATUS_CLOSED: CAN_SHUTDOWNCIRCUITSTATUS_OPEN)
#define set_tsmsSafetyStatus(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_TSCHG_MS) == DRV_IO_ACTIVE) ? \
                                                    CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_crcRepo(m,b,n,s) set(m,b,n,s, BUILDINFO_REPO_SHA_CRC)
#define set_repoDirty(m,b,n,s) set(m,b,n,s, BUILDINFO_REPO_DIRTY)

#include "TemporaryStubbing.h"
