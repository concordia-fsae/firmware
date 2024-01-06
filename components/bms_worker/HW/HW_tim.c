/**
 * HW_timebase.c
 * Hardware timer and tick config
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "include/ErrorHandler.h"
#include "include/HW.h"
#include "stm32f1xx.h"
#include "HW_tim.h"
#include "SystemConfig.h"
#include <stdint.h>


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim4;

static uint64_t fast_clk; /**< Stored in 20us/bit */

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

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
HAL_StatusTypeDef HW_TIM_Init()
{
    RCC_ClkInitTypeDef clkconfig;
    uint32_t           uwTimclock = 0;
    uint32_t           pFLatency;
    uint32_t uwPrescalerValue = 0;
   
    __HAL_RCC_TIM1_CLK_ENABLE();

    // Get clock configuration
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

    // Compute TIM4 clock
    uwTimclock       = HAL_RCC_GetPCLK2Freq();
    // Compute the prescaler value to have TIM4 counter clock equal to 2MHz
    uwPrescalerValue = (uint32_t)((uwTimclock / 2000000U) - 1U);

    /* USER CODE BEGIN TIM1_Init 0 */

    /* USER CODE END TIM1_Init 0 */

    TIM_ClockConfigTypeDef         sClockSourceConfig   = { 0 };
    TIM_MasterConfigTypeDef        sMasterConfig        = { 0 };
    TIM_OC_InitTypeDef             sConfigOC            = { 0 };
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = { 0 };

    /* USER CODE BEGIN TIM1_Init 1 */

    /* USER CODE END TIM1_Init 1 */
    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = uwPrescalerValue;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = 2000000/20000; 
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
    if (HAL_TIM_OC_Init(&htim1) != HAL_OK)
    {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigOC.OCMode       = TIM_OCMODE_PWM1;
    sConfigOC.Pulse        = 50;
    sConfigOC.OCPolarity   = TIM_OCPOLARITY_LOW;
    sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode   = TIM_OCFAST_ENABLE;
    sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_OC_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
    sBreakDeadTimeConfig.OffStateRunMode  = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTimeConfig.LockLevel        = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime         = 0;
    sBreakDeadTimeConfig.BreakState       = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity    = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.AutomaticOutput  = TIM_AUTOMATICOUTPUT_DISABLE;
    if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
    {
        Error_Handler();
    }    // Configure FAN PWM pin. Open-drain for sinking optoisolator
    
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitStruct.Pin = FAN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(FAN_GPIO_Port, &GPIO_InitStruct);
    
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    
    return HAL_OK;
}

void HW_TIM_ConfigureRunTimeStatsTimer(void)
{
//    RCC_ClkInitTypeDef clkconfig;
//    uint32_t           uwTimclock = 0;
//    uint32_t           pFLatency;
//    uint32_t uwPrescalerValue = 0;
//    
//    /**< RTOS Profiling Timer */
//    /**< Configure the TIM2 IRQ priority and enable */
//    HAL_NVIC_SetPriority(TIM2_IRQn, FAST_TICK_IRQ_PRIO, 0);
//    HAL_NVIC_EnableIRQ(TIM2_IRQn);
//
//    /**< Enable clock and initialize peripheral to be 10x RTOS clock */
//    /**< It is recommended to have 10-100x faster clock */
//    __HAL_RCC_TIM2_CLK_ENABLE();
//
//    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);
//    uwTimclock = 2 * HAL_RCC_GetPCLK1Freq();
//
//    /**< Set TIM2 to have 1MHz counter */
//    uwPrescalerValue = (uint32_t)((uwTimclock / 1000000U) - 1U);
//
//    htim2.Instance = TIM2;
//
//    // Initialize TIMx peripheral as follow:
//    // Period = [(TIM4CLK/1000) - 1]. to have a (1/10000)/2 s time base.
//    // Prescaler = (uwTimclock/1000000 - 1) to have a 1MHz counter clock.
//    // ClockDivision = 0
//    // Counter direction = Up
//    htim2.Init.Period        = (1000000U / 50000) - 1U;
//    htim2.Init.Prescaler     = uwPrescalerValue;
//    htim2.Init.ClockDivision = 0;
//    htim2.Init.CounterMode   = TIM_COUNTERMODE_UP;
//
//    if (HAL_TIM_Base_Init(&htim2) == HAL_OK)
//    {
//        // Start the TIM time Base generation in interrupt mode
//        if (HAL_TIM_Base_Start_IT(&htim2) == HAL_OK)
//        {
//            return;
//        }
//    }
//
//    Error_Handler();
}

void HW_TIM_IncBaseTick()
{
    fast_clk++;    
}

/**
 * @brief  RTOS Profiling has a ~1us accuracy by using the OS CLK and internal counter
 *
 * @retval   
 */
uint64_t HW_TIM_GetBaseTick()
{
    //return fast_clk;

    return (HW_GetTick() * 100) + htim1.Instance->CNT; 
}

void HW_TIM1_setDuty(uint8_t percentage)
{
    htim1.Instance->CCR1 = (uint16_t) (((uint32_t) percentage * htim1.Init.Period)/100);
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
    HAL_NVIC_SetPriority(TIM4_IRQn, TickPriority, 0);

    // Enable the TIM4 global Interrupt
    HAL_NVIC_EnableIRQ(TIM4_IRQn);

    // Enable TIM4 clock
    __HAL_RCC_TIM4_CLK_ENABLE();

    // Get clock configuration
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

    // Compute TIM4 clock
    uwTimclock = 2 * HAL_RCC_GetPCLK1Freq();
    // Compute the prescaler value to have TIM4 counter clock equal to 1MHz
    uwPrescalerValue = (uint32_t)((uwTimclock / 1000000U) - 1U);

    // Initialize TIM4
    htim4.Instance = TIM4;

    // Initialize TIMx peripheral as follow:
    // Period = [(TIM4CLK/1000) - 1]. to have a (1/10000) s time base.
    // Prescaler = (uwTimclock/1000000 - 1) to have a 1MHz counter clock.
    // ClockDivision = 0
    // Counter direction = Up
    htim4.Init.Period        = (1000000U / 10000) - 1U;
    htim4.Init.Prescaler     = uwPrescalerValue;
    htim4.Init.ClockDivision = 0;
    htim4.Init.CounterMode   = TIM_COUNTERMODE_UP;

    if (HAL_TIM_Base_Init(&htim4) == HAL_OK)
    {
        // Start the TIM time Base generation in interrupt mode
        return HAL_TIM_Base_Start_IT(&htim4);
    }

    return HAL_ERROR;
}

/**
 * HAL_SuspendTick
 * Suspend Tick increment
 * @note   Disable the tick increment by disabling TIM4 update interrupt.
 */
void HAL_SuspendTick(void)
{
    // Disable TIM4 update Interrupt
    __HAL_TIM_DISABLE_IT(&htim4, TIM_IT_UPDATE);
    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_UPDATE);
}


/**
 * HAL_ResumeTick
 * Resume Tick increment
 */
void HAL_ResumeTick(void)
{
    // Enable TIM4 Update interrupt
    __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);
}
