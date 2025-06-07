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
        .cs_amp_per_volt = 1.2048f,
        .cs_channel = DRV_INPUTAD_ANALOG_MUX_LP1_SNS,
        .diag_en = DRV_OUTPUTAD_DIA_EN,
        .sel1 = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2 = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch = DRV_OUTPUTAD_LP1_LATCH,
        .enable = {
            [DRV_TPS2HB16AB_OUT_1] = DRV_OUTPUTAD_BMS1_EN,
            [DRV_TPS2HB16AB_OUT_2] = DRV_OUTPUTAD_SHUTDOWN_EN,
        },
    },
    [DRV_TPS2HB16AB_IC_BMS2_ACCUM] = {
        .cs_amp_per_volt = 1.2048f,
        .cs_channel = DRV_INPUTAD_ANALOG_MUX_LP2_SNS,
        .diag_en = DRV_OUTPUTAD_DIA_EN,
        .sel1 = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2 = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch = DRV_OUTPUTAD_LP2_LATCH,
        .enable = {
            [DRV_TPS2HB16AB_OUT_1] = DRV_OUTPUTAD_BMS2_EN,
            [DRV_TPS2HB16AB_OUT_2] = DRV_OUTPUTAD_ACCUM_EN,
        },
    },
    [DRV_TPS2HB16AB_IC_BMS3_SENSOR] = {
        .cs_amp_per_volt = 1.2048f,
        .cs_channel = DRV_INPUTAD_ANALOG_MUX_LP3_SNS,
        .diag_en = DRV_OUTPUTAD_DIA_EN,
        .sel1 = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2 = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch = DRV_OUTPUTAD_LP3_LATCH,
        .enable = {
            [DRV_TPS2HB16AB_OUT_1] = DRV_OUTPUTAD_BMS3_EN,
            [DRV_TPS2HB16AB_OUT_2] = DRV_OUTPUTAD_SENSOR_EN,
        },
    },
    [DRV_TPS2HB16AB_IC_VC1_VC2] = {
        .cs_amp_per_volt = 1.2048f,
        .cs_channel = DRV_INPUTAD_ANALOG_MUX_LP4_SNS,
        .diag_en = DRV_OUTPUTAD_DIA_EN,
        .sel1 = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2 = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch = DRV_OUTPUTAD_LP4_LATCH,
        .enable = {
            [DRV_TPS2HB16AB_OUT_1] = DRV_OUTPUTAD_VC1_EN,
            [DRV_TPS2HB16AB_OUT_2] = DRV_OUTPUTAD_VC2_EN,
        },
    },
    [DRV_TPS2HB16AB_IC_MC_VCU3] = {
        .cs_amp_per_volt = 1.2048f,
        .cs_channel = DRV_INPUTAD_ANALOG_MUX_LP5_SNS,
        .diag_en = DRV_OUTPUTAD_DIA_EN,
        .sel1 = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2 = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch = DRV_OUTPUTAD_LP5_LATCH,
        .enable = {
            [DRV_TPS2HB16AB_OUT_1] = DRV_OUTPUTAD_MC_EN,
            [DRV_TPS2HB16AB_OUT_2] = DRV_OUTPUTAD_VCU3_EN,
        },
    },
    [DRV_TPS2HB16AB_IC_HVE_COCKPIT] = {
        .cs_amp_per_volt = 1.2048f,
        .cs_channel = DRV_INPUTAD_ANALOG_MUX_LP6_SNS,
        .diag_en = DRV_OUTPUTAD_DIA_EN,
        .sel1 = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2 = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch = DRV_OUTPUTAD_LP6_LATCH,
        .enable = {
            [DRV_TPS2HB16AB_OUT_1] = DRV_OUTPUTAD_HVE_EN,
            [DRV_TPS2HB16AB_OUT_2] = DRV_OUTPUTAD_COCKPIT_EN,
        },
    },
    [DRV_TPS2HB16AB_IC_SPARE_BMS4] = {
        .cs_amp_per_volt = 1.2048f,
        .cs_channel = DRV_INPUTAD_ANALOG_MUX_LP7_SNS,
        .diag_en = DRV_OUTPUTAD_DIA_EN,
        .sel1 = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2 = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch = DRV_OUTPUTAD_LP7_LATCH,
        .enable = {
            [DRV_TPS2HB16AB_OUT_1] = DRV_OUTPUTAD_SPARE_EN,
            [DRV_TPS2HB16AB_OUT_2] = DRV_OUTPUTAD_BMS4_EN,
        },
    },
    [DRV_TPS2HB16AB_IC_VCU1_VCU2] = {
        .cs_amp_per_volt = 1.2048f,
        .cs_channel = DRV_INPUTAD_ANALOG_MUX_LP8_SNS,
        .diag_en = DRV_OUTPUTAD_DIA_EN,
        .sel1 = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2 = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch = DRV_OUTPUTAD_LP8_LATCH,
        .enable = {
            [DRV_TPS2HB16AB_OUT_1] = DRV_OUTPUTAD_VCU1_EN,
            [DRV_TPS2HB16AB_OUT_2] = DRV_OUTPUTAD_VCU2_EN,
        },
    },
    [DRV_TPS2HB16AB_IC_BMS5_BMS6] = {
        .cs_amp_per_volt = 1.2048f,
        .cs_channel = DRV_INPUTAD_ANALOG_MUX_LP9_SNS,
        .diag_en = DRV_OUTPUTAD_DIA_EN,
        .sel1 = DRV_OUTPUTAD_MUX_LP_SEL1,
        .sel2 = DRV_OUTPUTAD_MUX_LP_SEL2,
        .latch = DRV_OUTPUTAD_LP9_LATCH,
        .enable = {
            [DRV_TPS2HB16AB_OUT_1] = DRV_OUTPUTAD_BMS5_EN,
            [DRV_TPS2HB16AB_OUT_2] = DRV_OUTPUTAD_BMS6_EN,
        },
    },
};
