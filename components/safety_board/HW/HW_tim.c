/**
 * @file HW_tim.c
 * @brief  Source code for the TIM Peripheral
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2023-03-10
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_tim.h"
#include "SYS_Vehicle.h"


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

TIM_HandleTypeDef htim;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void HW_TIM_Init(void)
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

    htim.Instance               = TIM1;
    htim.Init.Prescaler         = uwPrescalerValue;
    htim.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim.Init.Period            = 1000000000;
    htim.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim.Init.RepetitionCounter = 0;
    htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim) != HAL_OK)
    {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim, &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_IC_Init(&htim) != HAL_OK)
    {
        Error_Handler();
    }
    sSlaveConfig.SlaveMode        = TIM_SLAVEMODE_RESET;
    sSlaveConfig.InputTrigger     = TIM_TS_TI2FP2;
    sSlaveConfig.TriggerPolarity  = TIM_INPUTCHANNELPOLARITY_FALLING;
    sSlaveConfig.TriggerPrescaler = TIM_ICPSC_DIV1;
    sSlaveConfig.TriggerFilter    = 0;
    if (HAL_TIM_SlaveConfigSynchro(&htim, &sSlaveConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigIC.ICPolarity  = TIM_INPUTCHANNELPOLARITY_FALLING;
    sConfigIC.ICSelection = TIM_ICSELECTION_INDIRECTTI;
    sConfigIC.ICFilter    = 0;
    if (HAL_TIM_IC_ConfigChannel(&htim, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigIC.ICPolarity  = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    if (HAL_TIM_IC_ConfigChannel(&htim, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
    {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim, &sMasterConfig) != HAL_OK)
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

}


void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* tim)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    if (tim->Instance == TIM1)
    {
        __HAL_RCC_TIM1_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();
        /**TIM1 GPIO Configuration
        PA9     ------> TIM1_CH2
        */
        GPIO_InitStruct.Pin  = GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

void HW_TIM_Start(void)
{
    HAL_TIM_IC_Start_IT(&htim, TIM_CHANNEL_2);    // main channel
    HAL_TIM_IC_Start(&htim, TIM_CHANNEL_1);       // indirect channel
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef* tim)
{
    if (tim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)    // If the interrupt is triggered by channel 1
    {
        uint32_t ICValue, Duty, Frequency;
        // Read the IC value
        ICValue = HAL_TIM_ReadCapturedValue(tim, TIM_CHANNEL_2);

        if (ICValue != 0)
        {

            // calculate the Duty Cycle
            Duty = (HAL_TIM_ReadCapturedValue(tim, TIM_CHANNEL_1) * 100) / ICValue;


            //Weird black magic? Has todo with counter and etc
            Frequency = 5000000 / ICValue;

            SYS_SAFETY_SetIsolation((uint8_t)Duty, (uint8_t) (1000 / Frequency));
        }
    }

}
