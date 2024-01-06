/**
 * @file SystemManager.c
 * @brief Runs the Embedded System. Contains main called by the startup assembly.
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-28
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< FreeRTOS Includes */
#include "FreeRTOS.h"
#include "FreeRTOS_SWI.h"
#include "task.h"

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
#include "HW_MAX14921.h"
#include "HW_LTC2983.h"

/**< Other Includes */
#include "Module.h"
#include "Utility.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern void RTOS_createResources(void);


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * main
 * @return 0: Error
 */
int main(void)
{
    /**< Setup HAL Reset all peripherals. Initializes the Flash interface and the Systick. */
    HW_Init();
    
    /**< Configure system clocks */
    HW_SystemClock_Config();

    /**< Initiate Firmware */
    /**< Order is important, don't change without checking */
    HW_GPIO_Init();
    HW_TIM_Init();
    HW_I2C_Init();
    HW_CAN_Init();
    HW_DMA_Init();
    HW_ADC_Init();
    HW_SPI_Init();

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
 * @retval None
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
        while (cnt--);
    }
}

