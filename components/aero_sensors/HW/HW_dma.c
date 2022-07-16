/**
 * @file HW_dma.c
 * @brief  Source code for DMA hardware/firmware interface
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-16
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_dma.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void HW_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
}
