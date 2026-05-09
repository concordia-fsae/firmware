/**
 * @file drv_currentSense_componentSpecific.h
 * @brief Component specific header file for current sensing driver on the BMS board. (only 1 channel : Pack)
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
    DRV_CURRENTSENSE_CHANNEL_PACK,    // channel name = PACK
    DRV_CURRENTSENSE_CHANNEL_COUNT,   // channel counter = 1
}drv_currentSense_channel_E;