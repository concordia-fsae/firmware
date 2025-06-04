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

static uint64_t   RWheelSpeed_last_tick[2] = { 0 };
static uint64_t   LWheelSpeed_last_tick[2] = { 0 };

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
    //RCC_ClkInitTypeDef      clkconfig;
    TIM_ClockConfigTypeDef  sClockSourceConfig = { 0 };
    TIM_MasterConfigTypeDef sMasterConfig      = { 0 };
    TIM_IC_InitTypeDef      sConfigIC          = { 0 };
    uint32_t                uwTimclock         = 0;
   // uint32_t                pFLatency;
   // uint32_t                uwPrescalerValue   = 0;

    __HAL_RCC_TIM4_CLK_ENABLE();

    uwTimclock                   = HAL_RCC_GetPCLK1Freq();
    htim[HW_TIM_PORT_TACH].Instance               = TIM4;
    htim[HW_TIM_PORT_TACH].Init.Prescaler         = (uwTimclock / 1000000) - 1;
    htim[HW_TIM_PORT_TACH].Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim[HW_TIM_PORT_TACH].Init.Period            = 65535;
    htim[HW_TIM_PORT_TACH].Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim[HW_TIM_PORT_TACH].Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

        if (HAL_TIM_Base_Init(&htim[HW_TIM_PORT_TACH]) != HAL_OK)
    {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim[HW_TIM_PORT_TACH], &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_IC_Init(&htim[HW_TIM_PORT_TACH]) != HAL_OK)
    {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim[HW_TIM_PORT_TACH], &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigIC.ICPolarity  = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter    = 0;
    if (HAL_TIM_IC_ConfigChannel(&htim[HW_TIM_PORT_TACH], &sConfigIC, TIM_CHANNEL_3) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_IC_ConfigChannel(&htim[HW_TIM_PORT_TACH], &sConfigIC, TIM_CHANNEL_4) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_TIM_IC_Start_IT(&htim[HW_TIM_PORT_TACH], TIM_CHANNEL_3);    // main channel
    HAL_TIM_IC_Start_IT(&htim[HW_TIM_PORT_TACH], TIM_CHANNEL_4);    // main channel

    return HW_OK;
}

/**
 * @brief Deinitializes TIM peripheral
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_TIM_deInit(void)
{
    HAL_TIM_PWM_Stop(&htim[HW_TIM_PORT_TACH], TIM_CHANNEL_1);
    __HAL_RCC_TIM1_CLK_DISABLE();

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
    if (tim->Channel == HAL_TIM_ACTIVE_CHANNEL_3)    // If the interrupt is triggered by channel 1
    {
        RWheelSpeed_last_tick[0] = RWheelSpeed_last_tick[1];
        RWheelSpeed_last_tick[1] = HW_TIM_getBaseTick();
    }
    else if (tim->Channel == HAL_TIM_ACTIVE_CHANNEL_4)    // If the interrupt is triggered by channel 1
    {
        LWheelSpeed_last_tick[0] = LWheelSpeed_last_tick[1];
        LWheelSpeed_last_tick[1] = HW_TIM_getBaseTick();
    }
}

float32_t HW_TIM4_getFreqCH3(void)
{
    if ((RWheelSpeed_last_tick[1] + 1000000) < HW_TIM_getBaseTick()) 
    {
        return 0;
    }
    return (uint16_t)((RWheelSpeed_last_tick[1]) ? 1000000 / (RWheelSpeed_last_tick[1] - RWheelSpeed_last_tick[0]) : 0);
}

float32_t HW_TIM4_getFreqCH4(void)
{
    if ((LWheelSpeed_last_tick[1] + 1000000) < HW_TIM_getBaseTick()) 
    {
        return 0;
    }
    return (uint16_t)((LWheelSpeed_last_tick[1]) ? 1000000 / (LWheelSpeed_last_tick[1] - LWheelSpeed_last_tick[0]) : 0);
}

float32_t TIM4_getWheelSpeedCH3(void)
{
    return HW_TIM4_getFreqCH3() *1.237f; //1.237m = circumference of tire
}

float32_t TIM4_getWheelSpeedCH4(void)
{
    return HW_TIM4_getFreqCH4() *1.237f; //1.237m = circumference of tire
}
