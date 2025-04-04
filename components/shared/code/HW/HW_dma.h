/**
 * @file HW_dma.h
 * @brief  Header file for DMA firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "SystemConfig.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    BUFFER_HALF_LOWER = 0U,
    BUFFER_HALF_UPPER,
} HW_dma_bufferHalf_E;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void HW_DMA_init(void);
void HW_DMA_deInit(void);
