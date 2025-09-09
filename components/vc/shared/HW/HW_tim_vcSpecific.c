/**
 * @file HW_tim.c
 * @brief  Source code for TIM firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW_tim.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define TIMER_BASE_TICK 1000000U
#define SEC_TO_BASETICK(s) ((s) * TIMER_BASE_TICK)

#define TICK_PER_REV 16U
#define UPDATE_PER_REV 4U

#define NUM_TIM_SAMPLES 2U
#define CURRENT_SAMPLE (NUM_TIM_SAMPLES - 1U)
#define LAST_SAMPLE (NUM_TIM_SAMPLES - 2U)

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

TIM_HandleTypeDef htim[HW_TIM_PORT_COUNT];
typedef struct
{
    uint64_t lastSample[HW_TIM_CHANNEL_WS_CNT][NUM_TIM_SAMPLES];
} wheelSpeed_data_S;

static wheelSpeed_data_S wheelSpeed;

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
    TIM_ClockConfigTypeDef  sClockSourceConfig = { 0 };
    TIM_IC_InitTypeDef      sConfigIC          = { 0 };
    uint32_t                uwTimclock         = 0;

    __HAL_RCC_TIM4_CLK_ENABLE();

    uwTimclock                   = HAL_RCC_GetPCLK2Freq();
    htim[HW_TIM_PORT_WHEELSPEED].Instance               = TIM4;
    htim[HW_TIM_PORT_WHEELSPEED].Init.Prescaler         = (uwTimclock / TIMER_BASE_TICK) - 1;
    htim[HW_TIM_PORT_WHEELSPEED].Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim[HW_TIM_PORT_WHEELSPEED].Init.Period            = 65535;
    htim[HW_TIM_PORT_WHEELSPEED].Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim[HW_TIM_PORT_WHEELSPEED].Init.RepetitionCounter = 0;
    htim[HW_TIM_PORT_WHEELSPEED].Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim[HW_TIM_PORT_WHEELSPEED]) != HAL_OK)
    {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim[HW_TIM_PORT_WHEELSPEED], &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_IC_Init(&htim[HW_TIM_PORT_WHEELSPEED]) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigIC.ICPolarity  = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV4; // Driven by WHEELSPEED_TICK_PER_REV / UPDATE_PER_REV
    sConfigIC.ICFilter    = 0;
    if (HAL_TIM_IC_ConfigChannel(&htim[HW_TIM_PORT_WHEELSPEED], &sConfigIC, TIM_CHANNEL_3) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_IC_ConfigChannel(&htim[HW_TIM_PORT_WHEELSPEED], &sConfigIC, TIM_CHANNEL_4) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_TIM_IC_Start_IT(&htim[HW_TIM_PORT_WHEELSPEED], TIM_CHANNEL_3);
    HAL_TIM_IC_Start_IT(&htim[HW_TIM_PORT_WHEELSPEED], TIM_CHANNEL_4);

    return HW_OK;
}

/**
 * @brief  HAL callback once Initialization is complete. Used for GPIO/INTERRUPT configuration
 *
 * @param htim_base TIM peripheral
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* tim)
{
    if (tim->Instance == TIM4)
    {
        __HAL_RCC_TIM4_CLK_ENABLE();

        HAL_NVIC_SetPriority(TIM4_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(TIM4_IRQn);
    }
}

/**
 * @brief  HAL callback called once an input capture has triggered
 *
 * @param htim TIM peripheral
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef* tim)
{
    switch (tim->Channel)
    {
        case HAL_TIM_ACTIVE_CHANNEL_3:
            for (uint8_t i = 1; i < NUM_TIM_SAMPLES; i++)
            {
                wheelSpeed.lastSample[HW_TIM_CHANNEL_WS_R][i - 1U] = wheelSpeed.lastSample[HW_TIM_CHANNEL_WS_R][i];
            }
            wheelSpeed.lastSample[HW_TIM_CHANNEL_WS_R][CURRENT_SAMPLE] = HW_TIM_getBaseTick();
            break;
        case HAL_TIM_ACTIVE_CHANNEL_4:
            for (uint8_t i = 1; i < NUM_TIM_SAMPLES; i++)
            {
                wheelSpeed.lastSample[HW_TIM_CHANNEL_WS_L][i - 1U] = wheelSpeed.lastSample[HW_TIM_CHANNEL_WS_L][i];
            }
            wheelSpeed.lastSample[HW_TIM_CHANNEL_WS_L][CURRENT_SAMPLE] = HW_TIM_getBaseTick();
            break;
        default:
            break;
    }
}

float32_t HW_TIM_getFreq(HW_TIM_channelFreq_E channel)
{
    return (wheelSpeed.lastSample[channel][CURRENT_SAMPLE] > (HW_TIM_getBaseTick() - SEC_TO_BASETICK(1U))) ?
           (((float32_t)(SEC_TO_BASETICK(1U) / (UPDATE_PER_REV * 2.0f))) / (float32_t)(wheelSpeed.lastSample[channel][CURRENT_SAMPLE] - wheelSpeed.lastSample[channel][LAST_SAMPLE])) :
           0.0f;
}
