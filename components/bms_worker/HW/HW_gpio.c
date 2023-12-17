/**
 * HW_gpio.c
 * Hardware GPIO implementation
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_gpio.h"
#include "include/SystemConfig.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * MX_GPIO_Init
 * Configure pins as Analog, Input, Output, EVENT_OUT, or EXTI
 */
void HW_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    // Enable GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    // Configure LED pin Output Level
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

    // Configure SPI_NSS pin Output Level
//    HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);

    // Configure LED pin
    GPIO_InitStruct.Pin   = LED_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

    
//    // Configure SPI_NSS pin
//    GPIO_InitStruct.Pin   = SPI1_NSS_Pin;
//    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
//    GPIO_InitStruct.Pull  = GPIO_PULLUP;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//    HAL_GPIO_Init(SPI1_NSS_GPIO_Port, &GPIO_InitStruct);
//
//    // Configure GPIOB input pins
//    GPIO_InitStruct.Pin = SL_DATA_Pin | FT_PDN_Pin |
//                          BTN_MIDDLE_Pin | BTN_TOP_LEFT_Pin | BTN_TOP_RIGHT_Pin |
//                          ROT_ENC_1_A_Pin | ROT_ENC_1_B_Pin |
//                          ROT_ENC_2_A_Pin | ROT_ENC_2_B_Pin |
//                          SW1_Pin | SW2_Pin | SW4_Pin | SW5_Pin;
//    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
//    GPIO_InitStruct.Pull = GPIO_PULLUP;
//    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}
