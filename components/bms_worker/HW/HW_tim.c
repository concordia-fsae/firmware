/**
 * @file HW_tim.c
 * @brief  Source code for TIM firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "ErrorHandler.h"
#include "HW_tim.h"
#include "SystemConfig.h"
#include <stdint.h>

// Firmware Includes
#include "HW.h"
#include "stm32f1xx.h"


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
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
    GPIO_InitTypeDef        GPIO_InitStruct    = { 0 };
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
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV4;
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

    // Configure FAN PWM pin. Open-drain for sinking optoisolator
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin   = FAN1_PWM_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(FAN1_PWM_GPIO_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin   = FAN2_PWM_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(FAN2_PWM_GPIO_Port, &GPIO_InitStruct);

    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);

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
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    if (htim_base->Instance == TIM1)
    {
        __HAL_RCC_TIM1_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitStruct.Pin  = FAN1_PWM_Pin | FAN2_PWM_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
 * @brief  RTOS callback to configure a more precise timebase for cpu profiling
 */
void HW_TIM_configureRunTimeStatsTimer(void)
{
}

/**
 * @brief  RTOS Profiling has a ~1us accuracy by using the OS CLK and internal counter
 *
 * @retval Elapsed time in us from clock start
 */
uint64_t HW_TIM_getBaseTick()
{
    // return fast_clk;

    return (HW_getTick() * 100) + htim4.Instance->CNT;
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
    return (fan1_last_tick[1]) ? 2000000 / (fan1_last_tick[1] - fan1_last_tick[0]) : 0;
}

/**
 * @brief  Get input frequency from TIM1 CH2
 *
 * @retval Frequency of TIM1 CH2 input
 */
uint16_t HW_TIM1_getFreqCH2(void)
{
    if ((fan2_last_tick[1] + 1000000) < HW_TIM_getBaseTick()) 
    {
        return 0;
    }
    return (fan2_last_tick[1]) ? 2000000 / (fan2_last_tick[1] - fan2_last_tick[0]) : 0;
}


/**
 * HAL_InitTick
 * This function configures the TIM4 as a time base source.
 * The time source is configured  to have 1ms time base with a dedicated
 * Tick interrupt priority.
 * @note   This function is called  automatically at the beginning of program after
 *         reset by HAL_Init() or at any time when clock is configured, by HAL_RCC_ClockConfig().
 * @param  TickPriority Tick interrupt priority.
 * @return exit status
 */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    RCC_ClkInitTypeDef clkconfig;
    uint32_t           uwTimclock       = 0;
    uint32_t           uwPrescalerValue = 0;
    uint32_t           pFLatency;

    // Configure the TIM4 IRQ priority
    HAL_NVIC_SetPriority(TIM2_IRQn, TickPriority, 0);

    // Enable the TIM4 global Interrupt
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    // Enable TIM4 clock
    __HAL_RCC_TIM2_CLK_ENABLE();

    // Get clock configuration
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

    // Compute TIM2 clock
    uwTimclock       = 1 * HAL_RCC_GetPCLK2Freq();
    // Compute the prescaler value to have TIM4 counter clock equal to 1MHz
    uwPrescalerValue = (uint32_t)((uwTimclock / 1000000U) - 1U);

    // Initialize TIM4
    htim2.Instance   = TIM2;

    // Initialize TIMx peripheral as follow:
    // Period = [(TIM4CLK/1000) - 1]. to have a (1/10000) s time base.
    // Prescaler = (uwTimclock/1000000 - 1) to have a 1MHz counter clock.
    // ClockDivision = 0
    // Counter direction = Up
    htim2.Init.Period        = (1000000U / 10000) - 1U;
    htim2.Init.Prescaler     = uwPrescalerValue;
    htim2.Init.ClockDivision = 0;
    htim2.Init.CounterMode   = TIM_COUNTERMODE_UP;

    if (HAL_TIM_Base_Init(&htim2) == HAL_OK)
    {
        // Start the TIM time Base generation in interrupt mode
        return HAL_TIM_Base_Start_IT(&htim2);
    }

    return HAL_ERROR;
}

/**
 * @brief  Suspends the tick interrupt
 */
void HAL_SuspendTick(void)
{
    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_UPDATE);
}


/**
 * @brief  Resumes tick interrupt
 */
void HAL_ResumeTick(void)
{
    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);
}
