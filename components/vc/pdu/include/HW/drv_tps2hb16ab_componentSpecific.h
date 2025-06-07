/**
 * @file drv_tps2hb16ab_componentSpecific.h
 * @brief Header file for the TPS2HB16A/B high side drivers
 */

#pragma once

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_TPS2HB16AB_IC_BMS1_SHUTDOWN = 0x00U,
    DRV_TPS2HB16AB_IC_BMS2_ACCUM,
    DRV_TPS2HB16AB_IC_BMS3_SENSOR,
    DRV_TPS2HB16AB_IC_VC1_VC2,
    DRV_TPS2HB16AB_IC_MC_VCU3,
    DRV_TPS2HB16AB_IC_HVE_COCKPIT,
    DRV_TPS2HB16AB_IC_SPARE_BMS4,
    DRV_TPS2HB16AB_IC_VCU1_VCU2,
    DRV_TPS2HB16AB_IC_BMS5_BMS6,
    DRV_TPS2HB16AB_IC_COUNT,
} drv_tps2hb16ab_E;
