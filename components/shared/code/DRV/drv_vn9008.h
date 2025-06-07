/**
 * @file drv_vn9008.h
 * @brief Header file for the VN9008 high side driver
 *
 * Setup
 *
 * Usage
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_hsd.h"
#include "drv_inputAD.h"
#include "drv_outputAD.h"
#include "LIB_Types.h"
#include "lib_swFuse.h"
#include "drv_vn9008_componentSpecific.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t                     cs_amp_per_volt;
    drv_inputAD_channelAnalog_E   cs_channel;
    drv_outputAD_channelDigital_E fault_reset;
    drv_outputAD_channelDigital_E enable;
    drv_outputAD_channelDigital_E enable_cs;
    drv_outputAD_channelDigital_E sense_enable;
} drv_vn9008_channelConfig_S;

extern const drv_vn9008_channelConfig_S drv_vn9008_channels[DRV_VN9008_CHANNEL_COUNT];

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void            drv_vn9008_init(void);
void            drv_vn9008_run(void);
drv_hsd_state_E drv_vn9008_getState(drv_vn9008_E ic);
float32_t       drv_vn9008_getCurrent(drv_vn9008_E ic);
void            drv_vn9008_setEnabled(drv_vn9008_E ic, bool enabled);
void            drv_vn9008_setCSEnabled(drv_vn9008_E ic, bool enabled);
