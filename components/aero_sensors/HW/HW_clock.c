/**
 * @file HW_clock.c
 * @brief  Source code for firmware clock control
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-23
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_clock.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initialize the system clock to 72Mhz
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef       RCC_OscInitStruct = { 0 };
    RCC_ClkInitTypeDef       RCC_ClkInitStruct = { 0 };
    RCC_PeriphCLKInitTypeDef PeriphClkInit     = { 0 };

    /**
     * Initializes RCC to specified parameter
     * SYSCLK has freq = 64Mhz
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE; /**< HSE has 8Mhz Value */
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1; /**< Divides HSE OSC by 1 = 8Mhz */
    RCC_OscInitStruct.HSIState       = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE; /**< PLLSRC is 8Mhz */
    RCC_OscInitStruct.PLL.PLLMUL     = RCC_PLL_MUL8; /**< PLL has freq 64Mhz */
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /**
     * Initializes CPU, AHB, and APB bus clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK; /**< SYSCLOCK at 64Mhz */
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1; /**< AHB (bus, core, memory, and DMA) clocked at 64Mhz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2; /**< APB1 at 32Mhz clock */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1; /**< APB2 at 64Mhz clock */

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }

    /**
     * Peripheral clock initialization
     */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection    = RCC_ADCPCLK2_DIV8; /**< PCLK2 at 64Mhz, ADC clock at 8Mhz */
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM4 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    if (htim->Instance == TIM4) /**< TIM4 Initialization done in HW/HW_timebase.c */
    {
        HAL_IncTick(); 
    }
}


/**
 * @brief  Delay the code execution in blocking mode
 *
 * @param delay Time (ms)
 */
void HW_Delay(uint32_t delay)
{
    HAL_Delay(delay);
}

/**
 * @brief  Get the current tick counter
 *
 * @retval   Current tick count (ms)
 */
uint32_t HW_GetTick(void)
{
    return HAL_GetTick();
}
