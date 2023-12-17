/**
 * SystemManager for the Steering Wheel
 * starts the system
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// FreeRTOS includes
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_SWI.h"

// System includes
#include "stdbool.h"

// Other Includes
#include "HW_can.h"
#include "Module.h"
#include "HW_adc.h"
#include "HW_clock.h"
#include "HW_dma.h"
#include "HW_gpio.h"
#include "HW_spi.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern void RTOS_createResources(void);


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * main
 * @return TODO
 */
int main(void)
{
   // setup the system
   // Reset all peripherals, Initializes the Flash interface and the Systick.
    // Enable GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    // Configure LED pin Output Level
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
   HAL_Init();

   // Configure the system clock
   SystemClock_Config();

   // // Initialize all configured peripherals
   // // order is important here, don't change without checking
   // // TODO: change all of these from MX to HW (wtf does MX mean?)
   // MX_GPIO_Init();
   // HW_CAN_Init();
   // MX_DMA_Init();
   // MX_ADC1_Init();
   // MX_SPI1_Init();

   // // create the tasks, timers, etc.
   // RTOS_SWI_Init();
   // RTOS_createResources();

   // // run the module init
   // Module_init();

   // // init and start the scheduler
   // vTaskStartScheduler();

    return 0;
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
