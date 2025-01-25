/**
 * @file HW_tim.c
 * @brief  Source code for TIM firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "ErrorHandler.h"
#include "SystemConfig.h"
#include <stdint.h>

// Firmware Includes
#include "include/SystemConfig.h"
#include "stm32f1xx.h"
#include "HW.h"
#include "HW_tim.h"

#include "IMD.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;


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
    TIM_ClockConfigTypeDef  sClockSourceConfig = { 0 };
    TIM_SlaveConfigTypeDef  sSlaveConfig       = { 0 };
    TIM_IC_InitTypeDef      sConfigIC          = { 0 };
    TIM_MasterConfigTypeDef sMasterConfig      = { 0 };

    RCC_ClkInitTypeDef clkconfig;
    uint32_t uwTimclock       = 0;
    uint32_t uwPrescalerValue = 0;
    uint32_t pFLatency;

    // Get clock configuration
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);
    // Compute TIM4 clock
    uwTimclock       = 2 * HAL_RCC_GetPCLK2Freq();
    // Compute the prescaler value to have TIM4 counter clock equal to 1MHz
    uwPrescalerValue = (uint32_t)((uwTimclock / 1000000U) - 1U);

    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = uwPrescalerValue;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = 1000000000;
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
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
    sSlaveConfig.SlaveMode        = TIM_SLAVEMODE_RESET;
    sSlaveConfig.InputTrigger     = TIM_TS_TI2FP2;
    sSlaveConfig.TriggerPolarity  = TIM_INPUTCHANNELPOLARITY_FALLING;
    sSlaveConfig.TriggerPrescaler = TIM_ICPSC_DIV1;
    sSlaveConfig.TriggerFilter    = 0;
    if (HAL_TIM_SlaveConfigSynchro(&htim1, &sSlaveConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigIC.ICPolarity  = TIM_INPUTCHANNELPOLARITY_FALLING;
    sConfigIC.ICSelection = TIM_ICSELECTION_INDIRECTTI;
    sConfigIC.ICFilter    = 0;
    if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigIC.ICPolarity  = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
    {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }
    // Configure the TIM4 IRQ priority
    HAL_NVIC_SetPriority(TIM1_CC_IRQn, TICK_INT_PRIORITY, 0);
    HAL_NVIC_SetPriority(TIM1_BRK_IRQn, TICK_INT_PRIORITY, 0);
    HAL_NVIC_SetPriority(TIM1_UP_IRQn, TICK_INT_PRIORITY, 0);
    HAL_NVIC_SetPriority(TIM1_TRG_COM_IRQn, TICK_INT_PRIORITY, 0);

    // Enable the TIM4 global Interrupt
    HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);
    HAL_NVIC_EnableIRQ(TIM1_BRK_IRQn);
    HAL_NVIC_EnableIRQ(TIM1_UP_IRQn);
    HAL_NVIC_EnableIRQ(TIM1_TRG_COM_IRQn);

    HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_2);    // main channel
    HAL_TIM_IC_Start(&htim1, TIM_CHANNEL_1);       // indirect channel

    return HAL_OK;
}

/**
 * @brief  HAL callback once Initialization is complete. Used for GPIO/INTERRUPT configuration
 *
 * @param htim_base TIM peripheral
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* tim)
{
    if (tim->Instance == TIM1)
    {
        __HAL_RCC_TIM1_CLK_ENABLE();
    }
}

/**
 * @brief  HAL callback called once an input capture has triggered
 *
 * @param htim TIM peripheral
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef* htim)
{
    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)    // If the interrupt is triggered by channel 1
    {
        uint32_t ICValue, Duty, Frequency;
        // Read the IC value
        ICValue = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);

        if (ICValue != 0)
        {

            // calculate the Duty Cycle
            Duty = (HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1) * 100) / ICValue;


            //Weird black magic? Has todo with counter and etc
            Frequency = 500000 / ICValue;

            if (Frequency < 25) {
                float32_t res = (0.9f * 1200) / (((float32_t)Duty / 100) - 0.05f) - 1200.0f;
                IMD_setIsolation(res);
                IMD_setFault(false);
                IMD_setSST(true);
            }
            else if (Frequency >= 25 && Frequency <= 35)
            {
                if (Duty < 15) {
                    IMD_setSST(true);
                }
                else
                {
                    IMD_setSST(false);
                }
            }
            else if (Frequency > 35)
            {
                IMD_setFault(true);
            }
        }
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

    return ((uint64_t)HW_getTick() * 1000) + htim2.Instance->CNT;
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
