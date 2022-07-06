/**
 * @file HW_adc.c
 * @brief  Implementation of firmware/hardware adc interface
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-03
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"

#include "HW_adc.h"


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initializes the ADC1
 */
void HW_ADC1_Init()
{
    ADC_ChannelConfTypeDef adcConf = { 0 };

    /**< Initialize common ADC settings */
    hadc1.Instance                   = ADC1;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = ADC_SCAN_ENABLE;
    hadc1.Init.ContinuousConvMode    = ENABLE;
    hadc1.Init.NbrOfConversion       = 4;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    if (HAL_ADC_Init(&hadc1))
    {
        Error_Handler();
    }

    /**< Channel Configuration */
    adcConf.Channel      = ADC_CHANNEL_0;
    adcConf.Rank         = ADC_REGULAR_RANK_1;
    adcConf.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &adcConf))
    {
        Error_Handler();
    }

    adcConf.Channel      = ADC_CHANNEL_1;
    adcConf.Rank         = ADC_REGULAR_RANK_2;
    adcConf.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &adcConf))
    {
        Error_Handler();
    }

    adcConf.Channel      = ADC_CHANNEL_2;
    adcConf.Rank         = ADC_REGULAR_RANK_3;
    adcConf.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &adcConf))
    {
        Error_Handler();
    }

    adcConf.Channel      = ADC_CHANNEL_3;
    adcConf.Rank         = ADC_REGULAR_RANK_4;
    adcConf.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &adcConf))
    {
        Error_Handler();
    }
}

/**
 * @brief  Strong link to override __weak association of the HAL_ADC_MspDeInit()
 *          within HAL
 *
 * @param hadc Physical ADC to be deinitialized
 */
void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    if (hadc->Instance == ADC1)
    {
        /**< Enable ADC clock */
        __HAL_RCC_ADC1_CLK_ENABLE();

        /**< GPIOx clock should be enabled, but if not enable */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /**
         * ADC1 GPIO Configuration
         * PA0-WKUP     ------> ADC1_IN0 (AMUX_0)
         * PA1          ------> ADC1_IN1 (AMUX_1)
         * PA2          ------> ADC1_IN2 (AMUX_2)
         * PA3          ------> ADC1_IN8 (AMUX_3)
         */
        GPIO_InitStruct.Pin  = AMUX_1_Pin | AMUX_2_Pin | AMUX_3_Pin | AMUX_4_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /**
         * ADC1 configured on DMA Channel 1
         */
        // FIXME: Have in line with Current System
        hdma_adc1.Instance                 = DMA1_Channel1;
        hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
        hdma_adc1.Init.Mode                = DMA_CIRCULAR;
        hdma_adc1.Init.Priority            = DMA_PRIORITY_MEDIUM;
        if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
        {
            Error_Handler();
        }

        // FIXME: Have in line with current system
        __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);
    }
}

/**
 * @brief  Strong link to override __weak association of the HAL_ADC_MspInit()
 *          within HAL
 *
 * @param hadc Physical ADC to be Initialized
 */
void HAL_ADC_MspDeInit(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        /**< Disable Peripheral Clock */
        __HAL_RCC_ADC1_CLK_DISABLE();

        /**
         * ADC1 GPIO Configuration
         * PA0-WKUP     ------> ADC1_IN0
         * PA1          ------> ADC1_IN1
         * PA2          ------> ADC1_IN2
         * PB0          ------> ADC1_IN8
         * PB1          ------> ADC1_IN9
         */

        HAL_GPIO_DeInit(GPIOA, AMUX_1_Pin | AMUX_2_Pin | AMUX_3_Pin | AMUX_4_Pin);

        // ADC1 DMA DeInit
        // FIXME:Have in line with current system
        HAL_DMA_DeInit(hadc->DMA_Handle);
    }
}
