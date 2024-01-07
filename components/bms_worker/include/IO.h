/**
 * @file IO.h
 * @brief  Header file for IO Module
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-28
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Firmware includes */
#include "HW_adc.h"

/**< Other Includes */
#include "FloatTypes.h"
#include "Types.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define IO_ADC_BUF_LEN              264U    // number of samples to fill with DMA,
                                         // processed when half full and again when completely full


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    struct
    {
        float32_t mcu;
    } temp;

    uint8_t addr;
} IO_S;


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void IO_UnpackAdcBuffer(bufferHalf_E);
