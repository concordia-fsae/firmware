/**
 * @file SystemManager.c
 * @Synopsis  Starts system and runs scheduler
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-02
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"

#include "HW_clock.h"
#include "HW_gpio.h"
#include "HW_adc.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/


/**
 * @Synopsis  Main initialization function and scheduler start
 *
 * @Returns  Never returns 
 */
int main(void)
{
    HAL_Init(); /**< Initialize bare metal HAL system */

    SystemClock_Config(); /**< Configure system clock */

    /**< Initialize the hardware/firmware */
    HW_GPIO_Init();
    HW_ADC1_Init();

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 1);
    volatile uint32_t res = HAL_ADC_GetValue(&hadc1);
    res = res <<1;
    return 0;
}


/**
 * @Synopsis  Executed during error occurence
 */
void Error_Handler(void)
{
    __disable_irq();
    while(1)
    {
    }
}