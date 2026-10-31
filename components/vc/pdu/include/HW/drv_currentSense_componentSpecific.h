/**
 * @file drv_currentSense_componentSpecific.h
 * @brief Component specific header file for current sensing driver on the PDU board. Includes current sensing for powerManager.c, tps2hb16ab.c and vn90008.c
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_CURRENTSENSE_CHANNEL_PDU_5V,        // chan0 -> powerManager.c

    DRV_CURRENTSENSE_CHANNEL_PUMP,          // chan1 -> drv_vn9008.c
    DRV_CURRENTSENSE_CHANNEL_FAN,           // chan2 -> drv_vn9008.c

    DRV_CURRENTSENSE_CHANNEL_BMS1_SHUTDOWN, // chan3 -> drv_tps2hb16ab.c
    DRV_CURRENTSENSE_CHANNEL_BMS2_ACCUM,    // chan4 -> drv_tps2hb16ab.c
    DRV_CURRENTSENSE_CHANNEL_BMS3_SENSOR,   // chan5 -> drv_tps2hb16ab.c
    DRV_CURRENTSENSE_CHANNEL_VC1_VC2,       // chan6 -> drv_tps2hb16ab.c
    DRV_CURRENTSENSE_CHANNEL_MC_VCU3,       // chan7 -> drv_tps2hb16ab.c
    DRV_CURRENTSENSE_CHANNEL_HVE_COCKPIT,   // chan8 -> drv_tps2hb16ab.c
    DRV_CURRENTSENSE_CHANNEL_SPARE_BMS4,    // chan9 -> drv_tps2hb16ab.c
    DRV_CURRENTSENSE_CHANNEL_VCU1_VCU2,     // chan10 -> drv_tps2hb16ab.c
    DRV_CURRENTSENSE_CHANNEL_BMS5_BMS6,     // chan11 -> drv_tps2hb16ab.c

    DRV_CURRENTSENSE_CHANNEL_COUNT,         // channel count = 12
}drv_currentSense_channel_E;