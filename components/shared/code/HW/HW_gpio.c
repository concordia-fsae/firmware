/**
 * @file HW_gpio.c
 * @brief  Source code for GPIO firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW_gpio.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern const HW_GPIO_S HW_GPIO_pinmux[HW_GPIO_COUNT];

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Initializes GPIO peripheral
 *
 * @note Configure pins as Analog, Input, Output, EVENT_OUT, or EXTI
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_GPIO_init(void)
{
    // Enable GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    for (uint8_t pin = 0U; pin < HW_GPIO_COUNT; pin++)
    {
        if ((HW_GPIO_pinmux[pin].resetState != HW_GPIO_NOSET) &&
            (HW_GPIO_pinmux[pin].mode != GPIO_MODE_INPUT) &&
            (HW_GPIO_pinmux[pin].mode != GPIO_MODE_ANALOG))
        {
            HW_GPIO_writePin(pin, HW_GPIO_pinmux[pin].resetState == HW_GPIO_PINSET);
        }

        GPIO_InitTypeDef GPIO_InitStruct = { 0 };
        GPIO_InitStruct.Pin   = HW_GPIO_pinmux[pin].pin;
        GPIO_InitStruct.Mode  = HW_GPIO_pinmux[pin].mode;
        GPIO_InitStruct.Pull  = HW_GPIO_pinmux[pin].pull;
        GPIO_InitStruct.Speed = HW_GPIO_pinmux[pin].speed;
        HAL_GPIO_Init(HW_GPIO_pinmux[pin].port, &GPIO_InitStruct);
    }

    return HW_OK;
}

/**
 * @brief Deinitializes GPIO peripheral
 *
 * @retval HW_OK
 */

HW_StatusTypeDef_E HW_GPIO_deInit(void)
{
    for (uint8_t pin = 0U; pin < HW_GPIO_COUNT; pin++)
    {
        HAL_GPIO_DeInit(HW_GPIO_pinmux[pin].port, HW_GPIO_pinmux[pin].pin);
    }

    __HAL_RCC_GPIOA_CLK_DISABLE();
    __HAL_RCC_GPIOB_CLK_DISABLE();
    __HAL_RCC_GPIOC_CLK_DISABLE();
    __HAL_RCC_GPIOD_CLK_DISABLE();
    return HW_OK;
}

/**
 * @brief  Read's a GPIO pin state
 *
 * @param pin HW pin
 *
 * @retval   true = '1', false = '0'
 */
bool HW_GPIO_readPin(HW_GPIO_pinmux_E pin)
{
    return HAL_GPIO_ReadPin(HW_GPIO_pinmux[pin].port, HW_GPIO_pinmux[pin].pin) == GPIO_PIN_SET;
}

/**
 * @brief  Set's a GPIO pin state
 *
 * @param pin HW pin
 * @param state true = '1', false = '0'
 */
void HW_GPIO_writePin(HW_GPIO_pinmux_E pin, bool state)
{
    HAL_GPIO_WritePin(HW_GPIO_pinmux[pin].port, HW_GPIO_pinmux[pin].pin, (state) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief  Toggle's GPIO pin output
 *
 * @param pin HW pin
 */
void HW_GPIO_togglePin(HW_GPIO_pinmux_E pin)
{
    HAL_GPIO_TogglePin(HW_GPIO_pinmux[pin].port, HW_GPIO_pinmux[pin].pin);
}
