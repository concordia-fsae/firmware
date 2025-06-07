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
#define set_din1Debug(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_CHANNEL_DIN1) == DRV_IO_ACTIVE) ? \
                                             CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_din2Debug(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_CHANNEL_DIN2) == DRV_IO_ACTIVE) ? \
                                             CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_din3Debug(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_CHANNEL_DIN3) == DRV_IO_ACTIVE) ? \
                                             CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_din4Debug(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_CHANNEL_DIN4) == DRV_IO_ACTIVE) ? \
                                             CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_din5Debug(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_CHANNEL_DIN5) == DRV_IO_ACTIVE) ? \
                                             CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_din6Debug(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_CHANNEL_DIN6) == DRV_IO_ACTIVE) ? \
                                             CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_din7Debug(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_CHANNEL_DIN7) == DRV_IO_ACTIVE) ? \
                                             CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_din8Debug(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_CHANNEL_DIN8) == DRV_IO_ACTIVE) ? \
                                             CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_din9Debug(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_CHANNEL_DIN9) == DRV_IO_ACTIVE) ? \
                                             CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)
#define set_din10Debug(m,b,n,s) set(m,b,n,s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_CHANNEL_DIN10) == DRV_IO_ACTIVE) ? \
                                              CAN_DIGITALSTATUS_ON: CAN_DIGITALSTATUS_OFF)

#include "TemporaryStubbing.h"
