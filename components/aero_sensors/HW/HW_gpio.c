/**
 * @file HW_gpio.c
 * @brief  Implementation of firmware interface of aero sensor component
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-02
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"

#include "HW_gpio.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initializes the IO pins of the component
 */
void HW_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    // Enable GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    // Turn LED off
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    // Configure LED pin
    GPIO_InitStruct.Pin   = LED_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

    /**< Initialize I2C1 */
    AFIO->MAPR |= 0 << AFIO_MAPR_I2C1_REMAP_Pos; /**< No remap */

    GPIO_InitStruct.Pin = I2C1_SCL_Pin | I2C1_SDA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C1_SCL_Port, &GPIO_InitStruct);
    // TODO: Setup MUX selection outputs
}
