/**
 * @file drv_tps2hb16ab_componentSpecific.c
 * @brief Source file for the TPS2HB16A/B high side drivers
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_tps2hb16ab.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

const drv_tps2hb16ab_ic_S drv_tps2hb16ab_ics[DRV_TPS2HB16AB_IC_COUNT] = {
    [DRV_TPS2HB16AB_IC_BMS1_SHUTDOWN] = {
        .current_sense_channel = DRV_CURRENTSENSE_CHANNEL_BMS1_SHUTDOWN,
        .diag_en               = DRV_OUTPUTAD_DIA_EN,
        .sel1                  = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2                  = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch                 = DRV_OUTPUTAD_LP1_LATCH,
        .channel               =        {
            [DRV_TPS2HB16AB_OUT_1] =    {
                .enable            = DRV_OUTPUTAD_BMS1_EN,
                .current_limit_amp =                                   2.5f,
                .oc_timeout_ms     =                                    250,
            },
            [DRV_TPS2HB16AB_OUT_2] =    {
                .enable            = DRV_OUTPUTAD_SHUTDOWN_EN,
                .current_limit_amp =                                   2.5f,
                .oc_timeout_ms     =                                    250,
            },
        },
    },
    [DRV_TPS2HB16AB_IC_BMS2_ACCUM] =    {
        .current_sense_channel = DRV_CURRENTSENSE_CHANNEL_BMS2_ACCUM,
        .diag_en               = DRV_OUTPUTAD_DIA_EN,
        .sel1                  = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2                  = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch                 = DRV_OUTPUTAD_LP2_LATCH,
        .channel               =        {
            [DRV_TPS2HB16AB_OUT_1] =    {
                .enable            = DRV_OUTPUTAD_BMS2_EN,
                .current_limit_amp =                                2.5f,
                .oc_timeout_ms     =                                 250,
            },
            [DRV_TPS2HB16AB_OUT_2] =    {
                .enable            = DRV_OUTPUTAD_ACCUM_EN,
                .current_limit_amp =                                2.5f,
                .oc_timeout_ms     =                                 250,
            },
        },
    },
    [DRV_TPS2HB16AB_IC_BMS3_SENSOR] =   {
        .current_sense_channel = DRV_CURRENTSENSE_CHANNEL_BMS3_SENSOR,
        .diag_en               = DRV_OUTPUTAD_DIA_EN,
        .sel1                  = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2                  = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch                 = DRV_OUTPUTAD_LP3_LATCH,
        .channel               =        {
            [DRV_TPS2HB16AB_OUT_1] =    {
                .enable            = DRV_OUTPUTAD_BMS3_EN,
                .current_limit_amp =                                 2.5f,
                .oc_timeout_ms     =                                  250,
            },
            [DRV_TPS2HB16AB_OUT_2] =    {
                .enable            = DRV_OUTPUTAD_SENSOR_EN,
                .current_limit_amp =                                 2.5f,
                .oc_timeout_ms     =                                  250,
            },
        },
    },
    [DRV_TPS2HB16AB_IC_VC1_VC2] =       {
        .current_sense_channel = DRV_CURRENTSENSE_CHANNEL_VC1_VC2,
        .diag_en               = DRV_OUTPUTAD_DIA_EN,
        .sel1                  = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2                  = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch                 = DRV_OUTPUTAD_LP4_LATCH,
        .channel               =        {
            [DRV_TPS2HB16AB_OUT_1] =    {
                .enable            = DRV_OUTPUTAD_VC1_EN,
                .current_limit_amp =                             2.5f,
                .oc_timeout_ms     =                              250,
            },
            [DRV_TPS2HB16AB_OUT_2] =    {
                .enable            = DRV_OUTPUTAD_VC2_EN,
                .current_limit_amp =                             2.5f,
                .oc_timeout_ms     =                              250,
            },
        },
    },
    [DRV_TPS2HB16AB_IC_MC_VCU3] =       {
        .current_sense_channel = DRV_CURRENTSENSE_CHANNEL_MC_VCU3,
        .diag_en               = DRV_OUTPUTAD_DIA_EN,
        .sel1                  = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2                  = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch                 = DRV_OUTPUTAD_LP5_LATCH,
        .channel               =        {
            [DRV_TPS2HB16AB_OUT_1] =    {
                .enable            = DRV_OUTPUTAD_MC_EN,
                .current_limit_amp =                             2.5f,
                .oc_timeout_ms     =                              250,
            },
            [DRV_TPS2HB16AB_OUT_2] =    {
                .enable            = DRV_OUTPUTAD_VCU3_EN,
                .current_limit_amp =                             2.5f,
                .oc_timeout_ms     =                              250,
            },
        },
    },
    [DRV_TPS2HB16AB_IC_HVE_COCKPIT] =   {
        .current_sense_channel = DRV_CURRENTSENSE_CHANNEL_HVE_COCKPIT,
        .diag_en               = DRV_OUTPUTAD_DIA_EN,
        .sel1                  = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2                  = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch                 = DRV_OUTPUTAD_LP6_LATCH,
        .channel               =        {
            [DRV_TPS2HB16AB_OUT_1] =    {
                .enable            = DRV_OUTPUTAD_HVE_EN,
                .current_limit_amp =                                 2.5f,
                .oc_timeout_ms     =                                  250,
            },
            [DRV_TPS2HB16AB_OUT_2] =    {
                .enable            = DRV_OUTPUTAD_COCKPIT_EN,
                .current_limit_amp =                                 2.5f,
                .oc_timeout_ms     =                                  250,
            },
        },
    },
    [DRV_TPS2HB16AB_IC_SPARE_BMS4] =    {
        .current_sense_channel = DRV_CURRENTSENSE_CHANNEL_SPARE_BMS4,
        .diag_en               = DRV_OUTPUTAD_DIA_EN,
        .sel1                  = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2                  = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch                 = DRV_OUTPUTAD_LP7_LATCH,
        .channel               =        {
            [DRV_TPS2HB16AB_OUT_1] =    {
                .enable            = DRV_OUTPUTAD_SPARE_EN,
                .current_limit_amp =                                2.5f,
                .oc_timeout_ms     =                                 250,
            },
            [DRV_TPS2HB16AB_OUT_2] =    {
                .enable            = DRV_OUTPUTAD_BMS4_EN,
                .current_limit_amp =                                2.5f,
                .oc_timeout_ms     =                                 250,
            },
        },
    },
    [DRV_TPS2HB16AB_IC_VCU1_VCU2] =     {
        .current_sense_channel = DRV_CURRENTSENSE_CHANNEL_VCU1_VCU2,
        .diag_en               = DRV_OUTPUTAD_DIA_EN,
        .sel1                  = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2                  = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch                 = DRV_OUTPUTAD_LP8_LATCH,
        .channel               =        {
            [DRV_TPS2HB16AB_OUT_1] =    {
                .enable            = DRV_OUTPUTAD_VCU1_EN,
                .current_limit_amp =                               2.5f,
                .oc_timeout_ms     =                                250,
            },
            [DRV_TPS2HB16AB_OUT_2] =    {
                .enable            = DRV_OUTPUTAD_VCU2_EN,
                .current_limit_amp =                               2.5f,
                .oc_timeout_ms     =                                250,
                .default_enabled   = true,
            },
        },
    },
    [DRV_TPS2HB16AB_IC_BMS5_BMS6] =     {
        .current_sense_channel = DRV_CURRENTSENSE_CHANNEL_BMS5_BMS6,
        .diag_en               = DRV_OUTPUTAD_DIA_EN,
        .sel1                  = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2                  = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch                 = DRV_OUTPUTAD_LP9_LATCH,
        .channel               =        {
            [DRV_TPS2HB16AB_OUT_1] =    {
                .enable            = DRV_OUTPUTAD_BMS5_EN,
                .current_limit_amp =                               2.5f,
                .oc_timeout_ms     =                                250,
            },
            [DRV_TPS2HB16AB_OUT_2] =    {
                .enable            = DRV_OUTPUTAD_BMS6_EN,
                .current_limit_amp =                               2.5f,
                .oc_timeout_ms     =                                250,
            },
        },
    },
};
