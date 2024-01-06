/**
 * HW_dma.h
 * Header file for the DMA hardware implementation
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW.h"
#include "SystemConfig.h"


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HW_StatusTypeDef_E HW_DMA_Init(void);
HW_StatusTypeDef_E HW_DMA_DeInit(void);
