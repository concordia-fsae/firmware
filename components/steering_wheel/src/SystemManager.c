/*
 * SystemManager for the Steering Wheel
 * starts the system
 */

// FreeRTOS includes
#include "FreeRTOS.h"
#include "task.h"

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

// externs
extern void RTOS_createResources(void);

int main(void)
{
    // setup the system
    // Reset all peripherals, Initializes the Flash interface and the Systick.
    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_CAN_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_SPI1_Init();

    // create the tasks, timers, etc.
    RTOS_createResources();

    // run the module init
    Module_init();

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
    while (1) {}
}
