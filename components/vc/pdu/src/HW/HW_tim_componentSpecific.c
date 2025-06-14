/**
 * @file HW_tim.c
 * @brief  Source code for TIM firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "SystemConfig.h"
#include "LIB_Types.h"

// Firmware Includes
#include "HW_tim.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

TIM_HandleTypeDef htim[HW_TIM_PORT_COUNT];

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
    RCC_ClkInitTypeDef      clkconfig;
    TIM_ClockConfigTypeDef  sClockSourceConfig = { 0 };
#if FEATURE_IS_DISABLED(FEATURE_PUMP_FULL_BEANS)
    TIM_OC_InitTypeDef      sConfigOC          = { 0 };
#endif
    uint32_t                uwTimclock         = 0;
    uint32_t                pFLatency;
    uint32_t                uwPrescalerValue   = 0;

    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

    __HAL_RCC_TIM3_CLK_ENABLE();
    uwTimclock                                   = HAL_RCC_GetPCLK2Freq();
    uwPrescalerValue                             = (uint32_t)((uwTimclock / 1000000U) - 1U);
    htim[HW_TIM_PORT_PUMP].Instance               = TIM3;
    htim[HW_TIM_PORT_PUMP].Init.Prescaler         = uwPrescalerValue;
    htim[HW_TIM_PORT_PUMP].Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim[HW_TIM_PORT_PUMP].Init.Period            = 1000000 / 10000;
    htim[HW_TIM_PORT_PUMP].Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim[HW_TIM_PORT_PUMP].Init.RepetitionCounter = 0;
    htim[HW_TIM_PORT_PUMP].Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim[HW_TIM_PORT_PUMP]) != HAL_OK)
    {
        Error_Handler();
    }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;

    if (HAL_TIM_ConfigClockSource(&htim[HW_TIM_PORT_PUMP], &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_TIM_OC_Init(&htim[HW_TIM_PORT_PUMP]) != HAL_OK)
    {
        Error_Handler();
    }

#if FEATURE_IS_DISABLED(FEATURE_PUMP_FULL_BEANS)
    sConfigOC.OCMode       = TIM_OCMODE_PWM1;
    sConfigOC.Pulse        = 0;
    sConfigOC.OCPolarity   = TIM_OCPOLARITY_LOW;
    sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode   = TIM_OCFAST_ENABLE;
    sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_OC_ConfigChannel(&htim[HW_TIM_PORT_PUMP], &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_TIM_Base_Start(&htim[HW_TIM_PORT_PUMP]);
    HAL_TIM_PWM_Start(&htim[HW_TIM_PORT_PUMP], TIM_CHANNEL_1);
#endif

    return HW_OK;
}

/**
 * @brief  HAL callback once Initialization is complete. Used for GPIO/INTERRUPT configuration
 *
 * @param htim_base TIM peripheral
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* tim)
{
    UNUSED(tim);
}

/**
 * @brief  HAL callback called once an input capture has triggered
 *
 * @param htim TIM peripheral
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef* tim)
{
    UNUSED(tim);
}
