/**
 * @file SystemManager.c
 * @brief Runs the Embedded System. Contains main called by the startup assembly.
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< System Includes */
#include "stdbool.h"

/**< Firmware Includes */
#include "HW.h"
#include "HW_adc.h"
#include "HW_can.h"
#include "HW_clock.h"
#include "HW_dma.h"
#include "HW_gpio.h"
#include "HW_i2c.h"
#include "HW_spi.h"
#include "HW_tim.h"

/**< Driver Includes */
#include "HW_Fans.h"
#include "HW_HS4011.h"
#include "HW_LTC2983.h"
#include "HW_MAX14921.h"
#include "HW_SHT40.h"

/**< FreeRTOS Includes */
#include "FreeRTOS.h"
#include "FreeRTOS_SWI.h"
#include "task.h"

/**< Other Includes */
#include "Module.h"
#include "Utility.h"


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


typedef struct
{
    const uint32_t appStart;
    const uint32_t appEnd;
    const uint32_t appCrcLocation;
} appDesc_S;

__attribute__((section(".appDescriptor")))
const appDesc_S appDesc = {
    .appStart       = (const uint32_t)&__app_start_addr,
    .appEnd         = (const uint32_t)&__app_end_addr,
    // .appCrcLocation = (const uint32_t)&__app_crc_addr,
    .appCrcLocation = (const uint32_t)&__app_end_addr,
};
/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Main function called by the bootloader
 *
 * @retval none
 */
int main(void)
{
    /**< Setup HAL Reset all peripherals. Initializes the Flash interface and the Systick. */
    HW_init();

    /**< Configure system clocks */
    HW_systemClockConfig();

    /**< Initiate Firmware */
    /**< Order is important, don't change without checking */
    HW_GPIO_init();
    HW_TIM_init();
    HW_I2C_init();
    HW_CAN_init();
    HW_DMA_init();
    HW_ADC_init();
    HW_SPI_init();

    /**< Crate RTOS Tasks, Timers, etc... */
    RTOS_SWI_Init();
    RTOS_createResources();

    /**< Initialize Modules */
    Module_Init();

    /**< Start RTOS task scheduler. Should never return */
    vTaskStartScheduler();

    return 0;
}

/**
 * @brief  This function is executed in case of error occurrence.
 */
void Error_Handler(void)
{
    __disable_irq();

    /**< Configure LED if not already done */
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitStruct.Pin   = LED_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

    /**< Fast Toggle LED */
    while (1)
    {
        uint32_t cnt = 6400000;
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        while (cnt--)
        {
            ;
        }
    }
}
