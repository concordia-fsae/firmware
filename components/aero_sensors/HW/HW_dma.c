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

    // DMA interrupt init
    // DMA1_Channel1_IRQn interrupt configuration
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, DMA_IRQ_PRIO, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    /**< I2C2 DMA IRQ */
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, DMA_IRQ_PRIO, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, DMA_IRQ_PRIO, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

    /**< I2C1 DMA IRQ */
    HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, DMA_IRQ_PRIO, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, DMA_IRQ_PRIO, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
}
