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
#include "HW_dma.h"
#include "HW_adc.h"
#include "HW_i2c.h"
#include "HW_spi.h"
#include "fatfs.h"

#include "FreeRTOS.h"
#include "FreeRTOS_SWI.h"
#include "Module.h"


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern void RTOS_createResources(void);


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

    /**< Initialize the hardware/firmware and system level resources */
    HW_GPIO_Init();
    HW_DMA_Init();
    HW_ADC_Init();
    HW_I2C_Init();
    HW_SPI_Init();
    FatFS_Init();

    /**< Create tasks, timers, swi */
    RTOS_SWI_Init();
    RTOS_createResources();

    /**< Initialize modules */
    Module_Init();

#if defined(I2C1_TEST)
    HAL_Delay(50);
    HW_I2C_Device_S  tmp = {
        .handle = &i2c1,
        .addr = 0x11<<1,
    };
    uint8_t tmp1 = 0x01;
    HW_I2C_Master_Write(&tmp, &tmp1, 1, 500);
#endif /**< I2C1_TEST */
#if defined(I2C2_TEST)
    HAL_Delay(50);
    HW_I2C_Device_S  tmp = {
        .handle = &i2c2,
        .addr = 0x12<<1,
    };
    uint8_t tmp2 = 0x02;
    HW_I2C_Master_Write(&tmp, &tmp2, 1, 500);
#endif /**< I2C2_TEST */

    /**< Initialize and start the RTOS scheduler */
    vTaskStartScheduler();

    /**< Should never reach here */

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
