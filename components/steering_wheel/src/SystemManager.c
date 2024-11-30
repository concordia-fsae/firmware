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
#include <stdbool.h>

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

// this needs to be defined for __libc_init_array() from newlib_nano to be happy
extern void _init(void);
void _init(void){}

extern void RTOS_createResources(void);

// defined by linker
extern const uint32_t __app_start_addr;
extern const uint32_t __app_end_addr;


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    const uint32_t appStart;
    const uint32_t appEnd;
    const uint32_t appCrcLocation;
} appDesc_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

__attribute__((section(".appDescriptor")))
const lib_app_appDesc_S appDesc = {
    .appStart       = (const uint32_t)&__app_start_addr,
    .appEnd         = (const uint32_t)&__app_end_addr,
    // .appCrcLocation = (const uint32_t)&__app_crc_addr,
    .appCrcLocation = (const uint32_t)&__app_end_addr,
};


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

    // init and start the scheduler
    vTaskStartScheduler();

    return 0;
}

/**
 * @brief  General error handler
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
