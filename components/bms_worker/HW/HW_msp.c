/**
 * HW_msp.c
 * Initialize global MSP
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stm32f1xx.h"
#include "SystemConfig.h"
/**
 * HAL_MspInit
 * Initializes the Global MSP.
 */
void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);

    // Enable SWD, disable JTAG
    __HAL_AFIO_REMAP_SWJ_NOJTAG(); //NJTRST();
}
