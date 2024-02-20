/**
 * @file HW_msp.c
 * @brief  Source code for generic Msp firmware calls
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "SystemConfig.h"


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Overrides weak HAL Link for our implementation
 */
void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);

    // Enable SWD, disable JTAG
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}
