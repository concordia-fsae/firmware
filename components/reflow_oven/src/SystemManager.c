/**
 * @file SystemManager.c
 * @brief  System Manager of the reflow oven
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2023-01-13
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "ErrorHandler.h"
#include "SystemConfig.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Main function of the reflow oven
 *
 * @retval none
 */
int main(void)
{
    HAL_Init();

    return 0;
}

/**
 * @brief  Error handler of the reflow oven
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

