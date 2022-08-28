/**
 * @file HW_msp.c
 * @brief  Initialize global MSP
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-12
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Overrides __weak link from HAL library HAL_MspInit
 */
void HAL_MspInit(void)
{
    /**< Activate clocks */
    __HAL_RCC_PWR_CLK_ENABLE();

    // Enable SWD, disable JTAG
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}

