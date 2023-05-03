/**
 * @file HW_gpio.c
 * @brief  Source code for the GPIO firmware
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2023-01-15
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_gpio.h"
#include "SystemConfig.h"

#include "SYS_Vehicle.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  GPIO Initilization code
 */
void HW_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    /**< Enable Clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /**< Toggle output pins in correct state */
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BMS_STATUS_Port, BMS_STATUS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(IMD_STATUS_Port, IMD_STATUS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AIR_Port, AIR_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PCHG_Port, PCHG_Pin, GPIO_PIN_RESET);

    /**< Configure LED output */
    GPIO_InitStruct.Pin   = LED_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

    /**< Configure TSMS input, no pulldown required due to circuit pulldown */
    GPIO_InitStruct.Pin  = TSMS_CHG_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(TSMS_CHG_Port, &GPIO_InitStruct);

    /**< Configure OK_HS input, no pulldown required due to circuit pulldown */
    GPIO_InitStruct.Pin  = OK_HS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(OK_HS_Port, &GPIO_InitStruct);

    /**< Configure Status and Control signals */
    GPIO_InitStruct.Pin  = (BMS_STATUS_Pin | IMD_STATUS_Pin);
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = (AIR_Pin | PCHG_Pin);
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /**< Configure EXTI0 pin to be interrupt */
    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI0_IRQn, CYCLE_IRQ_PRIO, 0U);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, EXTI_IRQ_PRIO, 0U);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, EXTI_IRQ_PRIO, 0U);

    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/**
 * @brief  Interrupt to cycle state
 *
 * @param GPIO_Pin
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == TSMS_CHG_Pin)
    {
        if (HAL_GPIO_ReadPin(TSMS_CHG_Port, TSMS_CHG_Pin) == GPIO_PIN_RESET)
        {
            SYS_SAFETY_SetStatus(TSMS_STATUS, OFF);
        }
        else
        {
            SYS_SAFETY_SetStatus(TSMS_STATUS, ON);
        }
    }
    else if (GPIO_Pin == OK_HS_Pin)
    {
        if (HAL_GPIO_ReadPin(OK_HS_Port, OK_HS_Pin) == GPIO_PIN_RESET)
        {
            SYS_SAFETY_SetStatus(IMD_STATUS, OFF);
        }
        else
        {
            SYS_SAFETY_SetStatus(IMD_STATUS, ON);
        }
    }
    else if (GPIO_Pin == GPIO_PIN_0)
    {
        // Only set through internal software interrupts
        SYS_SAFETY_CycleState();
    }
}

uint8_t HW_GPIO_ValidateInputs(void)
{
    uint8_t ret = 0;

    if (HAL_GPIO_ReadPin(OK_HS_Port, OK_HS_Pin) == GPIO_PIN_SET)
    {
        ret |= 0x01 << IMD_STATUS;
    }
    
    if (HAL_GPIO_ReadPin(TSMS_CHG_Port, TSMS_CHG_Pin) == GPIO_PIN_SET)
    {
        ret |= 0x01 << TSMS_STATUS;
    }
    
    return ret;
}
