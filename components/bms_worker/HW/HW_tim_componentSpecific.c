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
#include "FeatureDefines_generated.h"

#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
#include "IO.h"
#endif // not FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

TIM_HandleTypeDef htim1;
#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
TIM_HandleTypeDef htim3;
#endif // FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
TIM_HandleTypeDef htim4;

static uint64_t   fan1_last_tick[2] = { 0 };
static uint64_t   fan2_last_tick[2] = { 0 };


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * @brief Initializes TIM peripheral
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_TIM_init(void)
{
    RCC_ClkInitTypeDef      clkconfig;
    TIM_ClockConfigTypeDef  sClockSourceConfig = { 0 };
    TIM_OC_InitTypeDef      sConfigOC          = { 0 };
    TIM_MasterConfigTypeDef sMasterConfig      = { 0 };
    TIM_IC_InitTypeDef      sConfigIC          = { 0 };
    uint32_t                uwTimclock         = 0;
    uint32_t                pFLatency;
    uint32_t                uwPrescalerValue   = 0;

    __HAL_RCC_TIM4_CLK_ENABLE();

    uwTimclock                   = HAL_RCC_GetPCLK2Freq();
    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = (uwTimclock / 2000000) - 1;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = 65535;
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
    {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_IC_Init(&htim1) != HAL_OK)
    {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigIC.ICPolarity  = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV8;
    sConfigIC.ICFilter    = 0;
    if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);    // main channel
    HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_2);    // main channel

    // Get clock configuration
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);
    uwTimclock                   = HAL_RCC_GetPCLK1Freq();
    uwPrescalerValue             = (uint32_t)((uwTimclock / 1000000U) - 1U);
    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = uwPrescalerValue;
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period            = 2000000 / 20000;
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.RepetitionCounter = 0;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
    {
        Error_Handler();
    }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;

    if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_TIM_OC_Init(&htim4) != HAL_OK)
    {
        Error_Handler();
    }

    sConfigOC.OCMode       = TIM_OCMODE_PWM1;
    sConfigOC.Pulse        = 0;
    sConfigOC.OCPolarity   = TIM_OCPOLARITY_LOW;
    sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode   = TIM_OCFAST_ENABLE;
    sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_OC_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigOC.OCMode       = TIM_OCMODE_PWM1;
    sConfigOC.Pulse        = 0;
    sConfigOC.OCPolarity   = TIM_OCPOLARITY_LOW;
    sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode   = TIM_OCFAST_ENABLE;
    sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_OC_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);

#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
    // Configure the TIM3 IRQ priority
    HAL_NVIC_SetPriority(TIM3_IRQn, TIM_IRQ_PRIO, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
    __HAL_RCC_TIM3_CLK_ENABLE();

    // Get clock configuration
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

    // Compute TIM2 clock
    uwTimclock       = 1 * HAL_RCC_GetPCLK2Freq();
    // Compute the prescaler value to have TIM4 counter clock equal to 1MHz
    uwPrescalerValue = (uint32_t)((uwTimclock / 1000000U) - 1U);

    // Initialize TIM4
    htim3.Instance   = TIM3;

    // Initialize TIMx peripheral as follow:
    // Period = [(TIM4CLK/1000) - 1]. to have a (1/10000) s time base.
    // Prescaler = (uwTimclock/1000000 - 1) to have a 1MHz counter clock.
    // ClockDivision = 0
    // Counter direction = Up
    htim3.Init.Period        = (1000000U / 5000) - 1U;
    htim3.Init.Prescaler     = uwPrescalerValue;
    htim3.Init.ClockDivision = 0;
    htim3.Init.CounterMode   = TIM_COUNTERMODE_UP;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
    {
        return HW_ERROR;
    }
#endif // FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
    return HW_OK;
}

/**
 * @brief Deinitializes TIM peripheral
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_TIM_deInit(void)
{
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    __HAL_RCC_TIM1_CLK_DISABLE();

    return HW_OK;
}

/**
 * @brief  HAL callback once Initialization is complete. Used for GPIO/INTERRUPT configuration
 *
 * @param htim_base TIM peripheral
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim_base)
{
    if (htim_base->Instance == TIM1)
    {
        __HAL_RCC_TIM1_CLK_ENABLE();

        HAL_NVIC_SetPriority(TIM1_CC_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);
    }
}

/**
 * @brief  HAL callback called once an input capture has triggered
 *
 * @param htim TIM peripheral
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef* htim)
{
    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)    // If the interrupt is triggered by channel 1
    {
        fan1_last_tick[0] = fan1_last_tick[1];
        fan1_last_tick[1] = HW_TIM_getBaseTick();
    }
    else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)    // If the interrupt is triggered by channel 1
    {
        fan2_last_tick[0] = fan2_last_tick[1];
        fan2_last_tick[1] = HW_TIM_getBaseTick();
    }
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM4 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 */
void HW_TIM_periodElapsedCb(TIM_HandleTypeDef* htim)
{
#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
    if (htim->Instance == TIM3)
    {
        HAL_TIM_Base_Stop_IT(&htim3);
        IO10kHz_CB();
    }
#endif
}

/**
 * @brief  Set duty cycle of TIM4 CH1 output
 *
 * @param percentage1 Duty cycle percentage. Unit: 1%
 */
void HW_TIM4_setDutyCH1(uint8_t percentage1)
{
    htim4.Instance->CCR1 = (uint16_t)(((uint32_t)percentage1 * htim4.Init.Period) / 100);
}

/* @brief  Set duty cycle of TIM4 CH2 output
 *
 * @param percentage2 Duty cycle percentage. Unit: 1%
 */
void HW_TIM4_setDutyCH2(uint8_t percentage2)
{
    htim4.Instance->CCR2 = (uint16_t)(((uint32_t)percentage2 * htim4.Init.Period) / 100);
}

/**
 * @brief  Get input frequency from TIM1 CH1
 *
 * @retval Frequency of TIM1 CH1 input
 */
uint16_t HW_TIM1_getFreqCH1(void)
{
    if ((fan1_last_tick[1] + 1000000) < HW_TIM_getBaseTick()) 
    {
        return 0;
    }
    return (uint16_t)((fan1_last_tick[1]) ? 4000000 / (fan1_last_tick[1] - fan1_last_tick[0]) : 0);
}

/**
 * @brief  Get input frequency from TIM1 CH2
 *
 * @retval Frequency of TIM1 CH2 input
 */
uint16_t HW_TIM1_getFreqCH2(void)
{
    if ((fan2_last_tick[1] + 1000000U) < HW_TIM_getBaseTick()) 
    {
        return 0;
    }
    return (uint16_t)((fan2_last_tick[1]) ? 4000000U / (fan2_last_tick[1] - fan2_last_tick[0]) : 0);
}

#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
void HW_TIM_10kHz_timerStart(void)
{
    HAL_TIM_Base_Start_IT(&htim3);
}
#endif // FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
