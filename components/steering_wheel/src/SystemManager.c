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

#include "NVM.h"
#include "Utility.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern void RTOS_createResources(void);


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

uint8_t test1 VAR_IN_SECTION("NVMRAM");
uint16_t test2 VAR_IN_SECTION("NVMRAM");
uint32_t test3 VAR_IN_SECTION("NVMRAM");
uint64_t test4 VAR_IN_SECTION("NVMRAM");

/**
 * main
 * @return TODO
 */
int main(void)
{
    // setup the system
    // Reset all peripherals, Initializes the Flash interface and the Systick.
    HAL_Init();

    // Configure the system clock
    SystemClock_Config();

    // Initialize all configured peripherals
    // order is important here, don't change without checking
    // TODO: change all of these from MX to HW (wtf does MX mean?)
    MX_GPIO_Init();
    HW_CAN_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_SPI1_Init();

    // create the tasks, timers, etc.
    RTOS_SWI_Init();
    RTOS_createResources();

    // run the module init
    Module_init();

    //nvm_init();
    HAL_StatusTypeDef status;
    status = nvm_clear_page(0);
    nvm_read_page(0);
    test1 = 1;
    test2 = 2;
    test3 = 3;
    test4 = 4;
    status = nvm_write_page(0);
    test1 = 0;
    test2 = 0;
    test3 = 0;
    test4 = 0;
    nvm_read_page(0);
    status += 1;

    // init and start the scheduler
    vTaskStartScheduler();

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
