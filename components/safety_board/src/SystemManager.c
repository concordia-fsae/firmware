/**
 * @file SystemManager.c
 * @brief  System Manager of the accumulator safety board
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2023-01-13
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "ErrorHandler.h"
#include "SystemConfig.h"

#include "HW_clock.h"
#include "HW_gpio.h"
#include "HW_can.h"

#include "SYS_Vehicle.h"

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
    
    SystemClock_Config();

    SYS_SAFETY_Init();
    HW_GPIO_Init();
    HW_CAN_Init();

    /**< Systenm stays in a hold state until any of the GPIO, CAN, or time based Interrupts
     * The system is completely reactive and fully interrupt driven
     */

    while(1);

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
