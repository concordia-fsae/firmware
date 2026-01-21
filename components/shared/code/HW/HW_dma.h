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

typedef enum
{
    HW_DMA_1 = 0x00U,
    HW_DMA_2,
    HW_DMA_COUNT,
} HW_dma_instance_E;

typedef enum
{
    HW_DMA_CHANNEL_NONE = 0x00U,
    HW_DMA_CHANNEL_1,
    HW_DMA_CHANNEL_2,
    HW_DMA_CHANNEL_3,
    HW_DMA_CHANNEL_4,
    HW_DMA_CHANNEL_5,
    HW_DMA_CHANNEL_6,
    HW_DMA_CHANNEL_7,
    HW_DMA_CHANNEL_8,
} HW_dma_channel_E;
#define HW_DMA1_CHANNEL_COUNT HW_DMA_CHANNEL_8
#define HW_DMA2_CHANNEL_COUNT HW_DMA_CHANNEL_5

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void HW_DMA_init(void);
void HW_DMA_deInit(void);
