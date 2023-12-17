/**
 * HW_adc.c
 * Hardware ADC implementation
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "FreeRTOS.h"
#include "task.h"

#include "HW_adc.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ADC_PRECALIBRATION_DELAY_ADCCLOCKCYCLES    2U
#define ADC_CALIBRATION_TIMEOUT                    10U

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * MX_ADC1_Init
 */
void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = { 0 };

    // Common config
    hadc1.Instance                   = ADC1;
    hadc1.Init.ScanConvMode          = ADC_SCAN_ENABLE;
    hadc1.Init.ContinuousConvMode    = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 6;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    // Configure Regular Channels

    sConfig.Channel      = ADC_CHANNEL_0;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.Channel      = ADC_CHANNEL_1;
    sConfig.Rank         = ADC_REGULAR_RANK_2;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.Channel      = ADC_CHANNEL_2;
    sConfig.Rank         = ADC_REGULAR_RANK_3;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.Channel      = ADC_CHANNEL_8;
    sConfig.Rank         = ADC_REGULAR_RANK_4;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.Channel      = ADC_CHANNEL_9;
    sConfig.Rank         = ADC_REGULAR_RANK_5;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.Channel      = ADC_CHANNEL_TEMPSENSOR;
    sConfig.Rank         = ADC_REGULAR_RANK_6;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * HAL_ADC_MspInit
 * @param adcHandle adc handle to operate on
 */
void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle)
{
  UNUSED(adcHandle);
//     GPIO_InitTypeDef GPIO_InitStruct = { 0 };
// 
//     if (adcHandle->Instance == ADC1)
//     {
//         // ADC1 clock enable
//         __HAL_RCC_ADC1_CLK_ENABLE();
// 
//         __HAL_RCC_GPIOA_CLK_ENABLE();
//         __HAL_RCC_GPIOB_CLK_ENABLE();
// 
//         /**
//          * ADC1 GPIO Configuration
//          * PA0-WKUP     ------> ADC1_IN0
//          * PA1          ------> ADC1_IN1
//          * PA2          ------> ADC1_IN2
//          * PB0          ------> ADC1_IN8
//          * PB1          ------> ADC1_IN9
//          */
//         GPIO_InitStruct.Pin  = CURR_SENSE_Pin | TEMP_BRD_Pin | TEMP_GPU_Pin;
//         GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
//         HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
// 
//         GPIO_InitStruct.Pin  = PADDLE_LEFT_Pin | PADDLE_RIGHT_Pin;
//         GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
//         HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
// 
//         // ADC1 DMA Init
//         // ADC1 Init
//         hdma_adc1.Instance                 = DMA1_Channel1;
//         hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
//         hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
//         hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
//         hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
//         hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
//         hdma_adc1.Init.Mode                = DMA_CIRCULAR;
//         hdma_adc1.Init.Priority            = DMA_PRIORITY_MEDIUM;
//         if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
//         {
//             Error_Handler();
//         }
// 
//         __HAL_LINKDMA(adcHandle, DMA_Handle, hdma_adc1);
//     }
}

/**
 * HAL_ADC_MspDeInit
 * @param adcHandle adc handle to operate on
 */
void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adcHandle)
{
    UNUSED(adcHandle);
//     if (adcHandle->Instance == ADC1)
//     {
//         // Peripheral clock disable
//         __HAL_RCC_ADC1_CLK_DISABLE();
// 
//         /**
//          * ADC1 GPIO Configuration
//          * PA0-WKUP     ------> ADC1_IN0
//          * PA1          ------> ADC1_IN1
//          * PA2          ------> ADC1_IN2
//          * PB0          ------> ADC1_IN8
//          * PB1          ------> ADC1_IN9
//          */
//         HAL_GPIO_DeInit(GPIOA, CURR_SENSE_Pin | TEMP_BRD_Pin | TEMP_GPU_Pin);
//         HAL_GPIO_DeInit(GPIOB, PADDLE_LEFT_Pin | PADDLE_RIGHT_Pin);
// 
//         // ADC1 DMA DeInit
//         HAL_DMA_DeInit(adcHandle->DMA_Handle);
//     }
}
