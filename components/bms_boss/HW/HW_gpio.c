/**
 * @file HW_gpio.c
 * @brief  Source code for GPIO firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "SystemConfig.h"

// Firmware Includes
#include "HW_gpio.h"

// Other Includes
#include "Sys.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initialization of GPIO pins.
 */
void HW_GPIO_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    // Enable GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    // Output pins default state and configuration
    HAL_GPIO_WritePin(BMS_STATUS_Port, BMS_STATUS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(IMD_STATUS_Port, IMD_STATUS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AIR_Port, AIR_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PCHG_Port, PCHG_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_Port, LED_Pin, GPIO_PIN_RESET);
    
    GPIO_InitStruct.Pin   = BMS_STATUS_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BMS_STATUS_Port, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin   = IMD_STATUS_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(IMD_STATUS_Port, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin   = AIR_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(AIR_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = PCHG_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PCHG_Port, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin   = LED_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_Port, &GPIO_InitStruct);

    // Input pins configuration
    GPIO_InitStruct.Pin   = OK_HS_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(OK_HS_Port, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin   = BMS_IMD_Reset_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(BMS_IMD_Reset_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = IMD_STATUS_MEM_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(IMD_STATUS_MEM_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = BMS_STATUS_MEM_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(BMS_STATUS_MEM_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = TSMS_CHG_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(TSMS_CHG_Port, &GPIO_InitStruct);
}

  /**
 * @brief  Deinitializes GPIO.
 */
void HW_GPIO_deInit(void)
{
}

/**
 * @brief  Read's a GPIO pin state
 *
 * @param dev HW pin
 *
 * @retval   true = '1', false = '0'
 */
bool HW_GPIO_readPin(HW_GPIO_S* dev)
{
    return HAL_GPIO_ReadPin(dev->port, dev->pin) == GPIO_PIN_SET;
}

/**
 * @brief  Set's a GPIO pin state
 *
 * @param dev HW pin
 * @param state true = '1', false = '0'
 */
void HW_GPIO_writePin(HW_GPIO_S* dev, bool state)
{
    HAL_GPIO_WritePin(dev->port, dev->pin, (state) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief  Toggle's GPIO pin output
 *
 * @param dev HW pin
 */
void HW_GPIO_togglePin(HW_GPIO_S* dev)
{
    HAL_GPIO_TogglePin(dev->port, dev->pin);
}
