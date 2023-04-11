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
#include "HW_tim.h"

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

    HW_GPIO_Init();
    HW_TIM_Init();
    HW_TIM_Start();
    HW_CAN_Init();
    HW_CAN_Start();
    SYS_SAFETY_Init();

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
    uint8_t c = 1;
    __disable_irq();
    while (c)
    {
    }
}
