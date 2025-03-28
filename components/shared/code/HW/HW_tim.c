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

TIM_HandleTypeDef htim_tick;
extern TIM_HandleTypeDef htim[HW_TIM_PORT_COUNT];

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/* @brief  Set duty cycle of TIM4 CH2 output
 * @param percentage2 Duty cycle percentage. Unit: 1%
 */
void HW_TIM_setDuty(HW_TIM_port_E port, HW_TIM_channel_E channel, float32_t percentage)
{
    switch (channel)
    {
        case HW_TIM_CHANNEL_1:
            htim[port].Instance->CCR1 = (uint16_t)(percentage * (float32_t)htim[port].Init.Period);
            break;
        case HW_TIM_CHANNEL_2:
            htim[port].Instance->CCR2 = (uint16_t)(percentage * (float32_t)htim[port].Init.Period);
            break;
        case HW_TIM_CHANNEL_3:
            htim[port].Instance->CCR3 = (uint16_t)(percentage * (float32_t)htim[port].Init.Period);
            break;
        case HW_TIM_CHANNEL_4:
            htim[port].Instance->CCR4 = (uint16_t)(percentage * (float32_t)htim[port].Init.Period);
            break;
    }
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM4 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 */
__weak void HW_TIM_periodElapsedCb(TIM_HandleTypeDef* tim)
{
    UNUSED(tim);
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called TIMx interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* tim)
{
    if (tim->Instance == HW_TIM_TICK)
    {
        HAL_IncTick();
    }
    else
    {
        HW_TIM_periodElapsedCb(tim);
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

    return ((uint64_t)HW_getTick() * 1000) + htim_tick.Instance->CNT;
}

/**
 * @brief  Get the number of ticks since clock start
 *
 * @retval Number of ticks
 */
uint32_t HW_TIM_getTick(void)
{
    return HAL_GetTick();
}

uint32_t HW_TIM_getTimeMS()
{
    return HW_TIM_getTick();
}

/**
 * HAL_InitTick
 * This function configures the HW_TIM_TICK as a time base source.
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

    // Configure the HW_TIM_TICK IRQ priority
    HAL_NVIC_SetPriority(HW_TIM_TICK_IRQN, TickPriority, 0);

    // Enable the HW_TIM_TICK global Interrupt
    HAL_NVIC_EnableIRQ(HW_TIM_TICK_IRQN);

    HW_TIM_TICK_ENABLECLK();

    // Get clock configuration
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

    // Compute TIM2 clock
    uwTimclock       = HW_TIM_TICK_GETCLKFREQ();
    // Compute the prescaler value to have HW_TIM_TICK counter clock equal to 1MHz
    uwPrescalerValue = (uint32_t)((uwTimclock / 1000000U) - 1U);

    // Initialize HW_TIM_TICK
    htim_tick.Instance = HW_TIM_TICK;

    // Initialize TIMx peripheral as follow:
    // Period = [(HW_TIM_TICK/1000) - 1]. to have a (1/1000) s time base.
    // Prescaler = (uwTimclock/1000000 - 1) to have a 1MHz counter clock.
    // ClockDivision = 0
    // Counter direction = Up
    htim_tick.Init.Period        = (1000000U / 1000) - 1U;
    htim_tick.Init.Prescaler     = uwPrescalerValue;
    htim_tick.Init.ClockDivision = 0;
    htim_tick.Init.CounterMode   = TIM_COUNTERMODE_UP;

    if (HAL_TIM_Base_Init(&htim_tick) == HAL_OK)
    {
        // Start the TIM time Base generation in interrupt mode
        return HAL_TIM_Base_Start_IT(&htim_tick);
    }

    return HAL_ERROR;
}

/**
 * @brief  Suspends the tick interrupt
 */
void HAL_SuspendTick(void)
{
    __HAL_TIM_DISABLE_IT(&htim_tick, TIM_IT_UPDATE);
}


/**
 * @brief  Resumes tick interrupt
 */
void HAL_ResumeTick(void)
{
    __HAL_TIM_ENABLE_IT(&htim_tick, TIM_IT_UPDATE);
}
