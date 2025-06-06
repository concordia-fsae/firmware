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
}

/**
 * @brief Deinitializes DMA peripheral
 *
 * @retval HW_OK
 */
void HW_DMA_deInit(void)
{
    __HAL_RCC_DMA1_CLK_DISABLE();
}
