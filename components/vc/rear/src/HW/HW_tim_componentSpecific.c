/**
 * @file HW_tim.c
 * @brief  Source code for TIM firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW_tim.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

TIM_HandleTypeDef htim[HW_TIM_PORT_COUNT];
static uint64_t   wheelSpeed_lastTick[2][2] = { 0 };

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initializes TIM peripherals
 *
 * @retval true = Success, false = Failure
 */
HW_StatusTypeDef_E HW_TIM_init(void)
{
    TIM_ClockConfigTypeDef  sClockSourceConfig = { 0 };
    TIM_IC_InitTypeDef      sConfigIC          = { 0 };
    uint32_t                uwTimclock         = 0;

    __HAL_RCC_TIM4_CLK_ENABLE();

    uwTimclock                   = HAL_RCC_GetPCLK2Freq();
    htim[HW_TIM_PORT_WHEELSPEED].Instance               = TIM4;
    htim[HW_TIM_PORT_WHEELSPEED].Init.Prescaler         = (uwTimclock / 10000) - 1;
    htim[HW_TIM_PORT_WHEELSPEED].Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim[HW_TIM_PORT_WHEELSPEED].Init.Period            = 10000;
    htim[HW_TIM_PORT_WHEELSPEED].Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim[HW_TIM_PORT_WHEELSPEED].Init.RepetitionCounter = 0;
    htim[HW_TIM_PORT_WHEELSPEED].Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim[HW_TIM_PORT_WHEELSPEED]) != HAL_OK)
    {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim[HW_TIM_PORT_WHEELSPEED], &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_IC_Init(&htim[HW_TIM_PORT_WHEELSPEED]) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigIC.ICPolarity  = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV4;
    sConfigIC.ICFilter    = 0;
    if (HAL_TIM_IC_ConfigChannel(&htim[HW_TIM_PORT_WHEELSPEED], &sConfigIC, TIM_CHANNEL_3) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_IC_ConfigChannel(&htim[HW_TIM_PORT_WHEELSPEED], &sConfigIC, TIM_CHANNEL_4) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_TIM_IC_Start_IT(&htim[HW_TIM_PORT_WHEELSPEED], TIM_CHANNEL_3);    // main channel
    HAL_TIM_IC_Start_IT(&htim[HW_TIM_PORT_WHEELSPEED], TIM_CHANNEL_4);    // main channel

    return HW_OK;
}

/**
 * @brief  HAL callback once Initialization is complete. Used for GPIO/INTERRUPT configuration
 *
 * @param htim_base TIM peripheral
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* tim)
{
    if (tim->Instance == TIM4)
    {
        __HAL_RCC_TIM4_CLK_ENABLE();

        HAL_NVIC_SetPriority(TIM4_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(TIM4_IRQn);
    }
}

/**
 * @brief  HAL callback called once an input capture has triggered
 *
 * @param htim TIM peripheral
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef* tim)
{
    if (tim->Channel == HAL_TIM_ACTIVE_CHANNEL_3)
    {
        wheelSpeed_lastTick[0][0] = wheelSpeed_lastTick[0][1];
        wheelSpeed_lastTick[0][1] = HW_TIM_getBaseTick();
    }
    else if (tim->Channel == HAL_TIM_ACTIVE_CHANNEL_4)
    {
        wheelSpeed_lastTick[1][0] = wheelSpeed_lastTick[1][1];
        wheelSpeed_lastTick[1][1] = HW_TIM_getBaseTick();
    }
}

float32_t HW_TIM4_getFreqCH3(void)
{
    if ((wheelSpeed_lastTick[0][1] + 1000000) < HW_TIM_getBaseTick()) 
    {
        return 0;
    }
    return (uint16_t)((wheelSpeed_lastTick[0][1]) ? 4000000 / (wheelSpeed_lastTick[0][1] - wheelSpeed_lastTick[0][0]) : 0);
}

float32_t HW_TIM4_getFreqCH4(void)
{
    if ((wheelSpeed_lastTick[1][1] + 1000000) < HW_TIM_getBaseTick()) 
    {
        return 0;
    }
    return (uint16_t)((wheelSpeed_lastTick[1][1]) ? 4000000 / (wheelSpeed_lastTick[1][1] - wheelSpeed_lastTick[1][0]) : 0);
}
