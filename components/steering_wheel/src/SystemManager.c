/*
 * SystemManager for the Steering Wheel
 * starts the system
 */

// FreeRTOS includes
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "task.h"

// System includes
#include "stdbool.h"

// Other Includes
#include "Module.h"
#include "adc.h"
#include "HW_can.h"
#include "clock.h"
#include "gpio.h"
#include "spi.h"

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
    MX_ADC1_Init();
    MX_ADC2_Init();
    MX_SPI1_Init();

    // create the tasks, timers, etc.
    RTOS_createResources();

    // run the module init
    Module_init();

    // init and start the scheduler
    osKernelInitialize();
    osKernelStart();

    while (true) {}
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}
