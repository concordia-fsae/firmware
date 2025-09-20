/**
 * @file drv_tps2hb16ab.h
 * @brief Header file for the TPS2HB16A/B high side drivers
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
#include "drv_tps2hb16ab_componentSpecific.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_TPS2HB16AB_OUT_1 = 0x00U,
    DRV_TPS2HB16AB_OUT_2,
    DRV_TPS2HB16AB_OUT_COUNT,
} drv_tps2hb16ab_output_E;

typedef struct
{
    float32_t                     cs_amp_per_volt;
    drv_inputAD_channelAnalog_E   cs_channel;
    drv_outputAD_channelDigital_E diag_en;
    drv_outputAD_channelDigital_E sel1;
    drv_outputAD_channelDigital_E sel2;
    drv_outputAD_channelDigital_E latch;
    struct {
        drv_outputAD_channelDigital_E enable;
        float32_t                     current_limit_amp;
        uint16_t                      oc_timeout_ms;
    } channel[DRV_TPS2HB16AB_OUT_COUNT];
} drv_tps2hb16ab_ic_S;

extern const drv_tps2hb16ab_ic_S drv_tps2hb16ab_ics[DRV_TPS2HB16AB_IC_COUNT];

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void            drv_tps2hb16ab_init(void);
void            drv_tps2hb16ab_run(void);
drv_hsd_state_E drv_tps2hb16ab_getState(drv_tps2hb16ab_E ic, drv_tps2hb16ab_output_E output);
float32_t       drv_tps2hb16ab_getCurrent(drv_tps2hb16ab_E ic, drv_tps2hb16ab_output_E output);
void            drv_tps2hb16ab_setEnabled(drv_tps2hb16ab_E ic, drv_tps2hb16ab_output_E output, bool enabled);
void            drv_tps2hb16ab_setDiagEnabled(drv_tps2hb16ab_E ic, bool enabled);
void            drv_tps2hb16ab_setCSChannel(drv_tps2hb16ab_E ic, drv_tps2hb16ab_output_E output);
void            drv_tps2hb16ab_setFaultLatch(drv_tps2hb16ab_E ic, bool latch_on_fault);
