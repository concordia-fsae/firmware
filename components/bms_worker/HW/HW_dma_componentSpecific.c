/**
 * @file HW_dma.c
 * @brief  Source code for DMA firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_dma.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Initializes DMA peripheral
 *
 * @retval HW_OK
 */
void HW_DMA_init(void)
{
    // DMA controller clock enable
    __HAL_RCC_DMA1_CLK_ENABLE();

    // DMA interrupt init
    // DMA1_Channel1_IRQn interrupt configuration
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, DMA_IRQ_PRIO, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

/**
 * @brief Deinitializes DMA peripheral
 *
 * @retval HW_OK
 */
void HW_DMA_deInit(void)
{

    HAL_NVIC_DisableIRQ(DMA1_Channel2_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);

    __HAL_RCC_DMA1_CLK_DISABLE();
}
