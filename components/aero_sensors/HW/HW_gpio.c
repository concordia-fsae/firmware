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

    // Turn Board LED off
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    // Configure Board LED pin
    GPIO_InitStruct.Pin   = LED_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

    // Turn Status LED off
    HAL_GPIO_WritePin(LED_R_Port, LED_R_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_G_Port, LED_G_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_B_Port, LED_B_Pin, GPIO_PIN_RESET);
    // Configure Status LED pin
    GPIO_InitStruct.Pin   = LED_R_Pin | LED_G_Pin | LED_B_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);
    
    /**< Initialize I2C1 */
    GPIO_InitStruct.Pin = I2C1_SCL_Pin | I2C1_SDA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C1_SCL_Port, &GPIO_InitStruct);
    
    /**< Initialize I2C2 */
    GPIO_InitStruct.Pin = I2C2_SCL_Pin | I2C2_SDA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C2_SCL_Port, &GPIO_InitStruct);
}

/**
 * @brief Reads the input pin state
 * @param[in] input Input pin
 * @return HW pin state
 */
HW_GPIO_PinState_E HW_GPIO_ReadPin(const HW_GPIO_S *input)
{
    /* Check the parameters */
    assert_param(IS_GPIO_PIN(input->pin));

    if ((input->port->IDR & input->pin) != (uint32_t)GPIO_PIN_RESET)
    {
        return HW_PIN_SET;
    }
    else
    {
        return HW_PIN_RESET;
    }
}

void HW_GPIO_WritePin(const HW_GPIO_S *output, HW_GPIO_PinState_E PinState)
{
  /* Check the parameters */
  assert_param(IS_GPIO_PIN(output->pin));
  assert_param(IS_GPIO_PIN_ACTION(PinState));

  if (PinState != HW_PIN_RESET)
  {
    output->port->BSRR = output->pin;
  }
  else
  {
    output->port->BSRR = (uint32_t)output->pin << 16u;
  }
}

