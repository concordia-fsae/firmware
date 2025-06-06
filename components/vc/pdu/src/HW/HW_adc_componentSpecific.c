/**
 * @file HW_adc_componentSpecific.c
 * @brief  Source code for ADC firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// FreeRTOS Includes
#include "FreeRTOS.h"
#include "task.h"

// System Inlcudes
#include "string.h"

// Firmware Includes
#include "HW_adc_private.h"
#include "HW_dma.h"

// Other Includes
#include "SystemConfig.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ADC_PRECALIBRATION_DELAY_ADCCLOCKCYCLES 2U
#define ADC_CALIBRATION_TIMEOUT                 10U

#define ADC_BANK1_CHANNEL_MUX_LP1_SNS       ADC_CHANNEL_7
#define ADC_BANK1_CHANNEL_MUX_LP2_SNS       ADC_CHANNEL_10
#define ADC_BANK1_CHANNEL_MUX_LP3_SNS       ADC_CHANNEL_11
#define ADC_BANK1_CHANNEL_MUX_LP4_SNS       ADC_CHANNEL_12
#define ADC_BANK1_CHANNEL_5V_VOLTAGE        ADC_CHANNEL_4
#define ADC_BANK1_CHANNEL_UVL_BATT          ADC_CHANNEL_1
#define ADC_BANK1_CHANNEL_MCU_TEMP          ADC_CHANNEL_TEMPSENSOR

#define ADC_BANK2_CHANNEL_MUX_LP5_SNS       ADC_CHANNEL_13
#define ADC_BANK2_CHANNEL_MUX_LP6_SNS       ADC_CHANNEL_14
#define ADC_BANK2_CHANNEL_MUX_LP7_SNS       ADC_CHANNEL_15
#define ADC_BANK2_CHANNEL_MUX_LP8_SNS       ADC_CHANNEL_8
#define ADC_BANK2_CHANNEL_MUX_LP9_SNS       ADC_CHANNEL_9
#define ADC_BANK2_CHANNEL_MUX2_HP_CS        ADC_CHANNEL_3
#define ADC_BANK2_CHANNEL_MUX2_THERMISTORS  ADC_CHANNEL_2

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

DMA_HandleTypeDef hdma_adc1;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Init function for ADC firmware
 */
HW_StatusTypeDef_E HW_ADC_init_componentSpecific(void)
{
    ADC_MultiModeTypeDef   multimode = { 0 };
    ADC_ChannelConfTypeDef sConfig   = { 0 };

    // Cell Measurement config
    hadc1.Instance                   = ADC1;
    hadc1.Init.ScanConvMode          = ADC_SCAN_ENABLE;
    hadc1.Init.ContinuousConvMode    = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 7;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    multimode.Mode = ADC_DUALMODE_REGSIMULT;
    if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
    {
        Error_Handler();
    }

    // Configure Regular Channels
    sConfig.Channel      = ADC_BANK1_CHANNEL_MUX_LP1_SNS;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK1_CHANNEL_MUX_LP2_SNS;
    sConfig.Rank         = ADC_REGULAR_RANK_2;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK1_CHANNEL_MUX_LP3_SNS;
    sConfig.Rank         = ADC_REGULAR_RANK_3;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK1_CHANNEL_MUX_LP4_SNS;
    sConfig.Rank         = ADC_REGULAR_RANK_4;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.Channel      = ADC_BANK1_CHANNEL_5V_VOLTAGE;
    sConfig.Rank         = ADC_REGULAR_RANK_5;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK1_CHANNEL_UVL_BATT;
    sConfig.Rank         = ADC_REGULAR_RANK_6;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK1_CHANNEL_MCU_TEMP;
    sConfig.Rank         = ADC_REGULAR_RANK_7;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    hadc2.Instance                   = ADC2;
    hadc2.Init.ScanConvMode          = ADC_SCAN_ENABLE;
    hadc2.Init.ContinuousConvMode    = ENABLE;
    hadc2.Init.DiscontinuousConvMode = DISABLE;
    hadc2.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc2.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc2.Init.NbrOfConversion       = 7;
    if (HAL_ADC_Init(&hadc2) != HAL_OK)
    {
        Error_Handler();
    }

    // Configure Regular Channels
    sConfig.Channel      = ADC_BANK2_CHANNEL_MUX_LP5_SNS;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK2_CHANNEL_MUX_LP6_SNS;
    sConfig.Rank         = ADC_REGULAR_RANK_2;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK2_CHANNEL_MUX_LP7_SNS;
    sConfig.Rank         = ADC_REGULAR_RANK_3;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK2_CHANNEL_MUX_LP8_SNS;
    sConfig.Rank         = ADC_REGULAR_RANK_4;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK2_CHANNEL_MUX_LP9_SNS;
    sConfig.Rank         = ADC_REGULAR_RANK_5;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK2_CHANNEL_MUX2_HP_CS;
    sConfig.Rank         = ADC_REGULAR_RANK_6;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK2_CHANNEL_MUX2_THERMISTORS;
    sConfig.Rank         = ADC_REGULAR_RANK_7;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    return HW_OK;
}

/**
 * @brief  Callback for STM32 HAL once ADC initialization is complete
 *
 * @param adcHandle pointer to ADC peripheral
 */
void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle)
{
    if (adcHandle->Instance == ADC1)
    {
        // ADC1 clock enable
        __HAL_RCC_ADC1_CLK_ENABLE();

        // ADC1 DMA Init
        // ADC1 Init
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

        __HAL_LINKDMA(adcHandle, DMA_Handle, hdma_adc1);

        HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, DMA_IRQ_PRIO, 0U);
        HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    }
    else if (adcHandle->Instance == ADC2)
    {
        // ADC1 clock enable
        __HAL_RCC_ADC2_CLK_ENABLE();
    }
}

/**
 * @brief  STM32 HAL callback. Called once de-init is complete
 *
 * @param adcHandle Pointer to ADC peripheral
 */
void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adcHandle)
{
    if (adcHandle->Instance == ADC1)
    {
        __HAL_RCC_ADC1_CLK_DISABLE();

        HAL_NVIC_DisableIRQ(DMA1_Channel2_IRQn);
        HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);
        // ADC1 DMA DeInit
        HAL_DMA_DeInit(adcHandle->DMA_Handle);
    }
    if (adcHandle->Instance == ADC2)
    {
        // Peripheral clock disable
        __HAL_RCC_ADC2_CLK_DISABLE();
    }
}
