/**
 * @file HW_tim.c
 * @brief  Source code for TIM firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "HW_tim.h"
#include "ErrorHandler.h"
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

static uint64_t imd1_last_tick[2] = { 0 };
static uint64_t imd2_last_tick[2] = { 0 };


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * @brief  Initializes TIM peripherals
 *
 * @retval true = Success, false = Failure
 */
HAL_StatusTypeDef HW_TIM_init(void)
{
    return HAL_OK;
}

/**
 * @brief  HAL callback once Initialization is complete. Used for GPIO/INTERRUPT configuration
 *
 * @param htim_base TIM peripheral
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim_base)
{
    UNUSED(htim_base);
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
        imd1_last_tick[0] = imd1_last_tick[1];
        imd1_last_tick[1] = HW_getTick() * 100 + HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
    }
    else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)    // If the interrupt is triggered by channel 1
    {
        imd2_last_tick[0] = imd2_last_tick[1];
        imd2_last_tick[1] = HW_getTick() * 100 + HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
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

    return (HW_getTick() * 1000) + htim4.Instance->CNT;
}

/**
 * @brief  Get input frequency from TIM1 CH1
 *
 * @retval Frequency of TIM1 CH1 input
 */
uint16_t HW_TIM1_getFreqCH1(void)
{
    return (imd1_last_tick[1]) ? 2000000 / (imd1_last_tick[1] - imd1_last_tick[0]) : 0;
}

/**
 * @brief  Get input frequency from TIM1 CH2
 *
 * @retval Frequency of TIM1 CH2 input
 */
uint16_t HW_TIM3_getFreqCH1(void)
{
    return (imd2_last_tick[1]) ? 2000000 / (imd2_last_tick[1] - imd2_last_tick[0]) : 0;
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
    htim2.Instance = TIM2;

    // Initialize TIMx peripheral as follow:
    // Period = [(TIM4CLK/1000) - 1]. to have a (1/1000) s time base.
    // Prescaler = (uwTimclock/1000000 - 1) to have a 1MHz counter clock.
    // ClockDivision = 0
    // Counter direction = Up
    htim2.Init.Period        = (1000000U / 1000) - 1U;
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
