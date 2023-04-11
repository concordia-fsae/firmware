/**
 * HW_timebase.c
 * Hardware timer and tick config
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stm32f1xx_hal.h"


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/
TIM_HandleTypeDef htim4;


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
    // Period = [(TIM4CLK/1000) - 1]. to have a (1/1000) s time base.
    // Prescaler = (uwTimclock/1000000 - 1) to have a 1MHz counter clock.
    // ClockDivision = 0
    // Counter direction = Up
    htim4.Init.Period        = (1000000U / 1000U) - 1U;
    htim4.Init.Prescaler     = uwPrescalerValue;
    htim4.Init.ClockDivision = 0;
    htim4.Init.CounterMode   = TIM_COUNTERMODE_UP;

    if (HAL_TIM_Base_Init(&htim4) == HAL_OK)
    {
        // Start the TIM time Base generation in interrupt mode
        return HAL_TIM_Base_Start_IT(&htim4);
    }

    // Return function status
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
}


/**
 * HAL_ResumeTick
 * Resume Tick increment
 */
void HAL_ResumeTick(void)
{
    // Enable TIM4 Update interrupt
    __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);
}
