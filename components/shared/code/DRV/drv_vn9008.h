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
#include "HW_tim.h"
#include "drv_vn9008_componentSpecific.h"
#include "lib_swFuse.h"
#include "LIB_Types.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    enum
    {
        VN9008_DIGITAL = 0x00,
        VN9008_PWM_EN,
    } type;
    union
    {
        drv_outputAD_channelDigital_E digital;
        struct
        {
            HW_TIM_pwmChannel_S pwm;
            drv_outputAD_channelDigital_E en;
        } pwm_en;
    } enable;
    float32_t                     cs_amp_per_volt;
    drv_inputAD_channelAnalog_E   cs_channel;
    drv_outputAD_channelDigital_E fault_reset;
    drv_outputAD_channelDigital_E enable_cs;
    float32_t                     current_limit_amp;
    uint16_t                      oc_timeout_ms;
} drv_vn9008_channelConfig_S;

extern const drv_vn9008_channelConfig_S drv_vn9008_channels[DRV_VN9008_CHANNEL_COUNT];

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void            drv_vn9008_init(void);
void            drv_vn9008_run(void);
drv_hsd_state_E drv_vn9008_getState(drv_vn9008_E ic);
float32_t       drv_vn9008_getCurrent(drv_vn9008_E ic);
float32_t       drv_vn9008_getDuty(drv_vn9008_E ic);
void            drv_vn9008_setDuty(drv_vn9008_E ic, float32_t duty);
void            drv_vn9008_setEnabled(drv_vn9008_E ic, bool enabled);
void            drv_vn9008_setCSEnabled(drv_vn9008_E ic, bool enabled);
