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

// Other Includes
#include "SystemConfig.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ADC_PRECALIBRATION_DELAY_ADCCLOCKCYCLES 2U
#define ADC_CALIBRATION_TIMEOUT                 10U

#define ADC_BANK1_CHANNEL_R_BR_TEMP             ADC_CHANNEL_7
#define ADC_BANK1_CHANNEL_L_SHK_DISP            ADC_CHANNEL_4
#define ADC_BANK1_CHANNEL_PU1                   ADC_CHANNEL_14
#define ADC_BANK1_CHANNEL_BR_POT                ADC_CHANNEL_3
#define ADC_BANK1_CHANNEL_SPARE1                ADC_CHANNEL_10
#define ADC_BANK1_CHANNEL_SPARE2                ADC_CHANNEL_11
#define ADC_BANK1_CHANNEL_APPS_P1               ADC_CHANNEL_1
#define ADC_BANK1_CHANNEL_MCU_TEMP              ADC_CHANNEL_TEMPSENSOR

#define ADC_BANK2_CHANNEL_L_BR_TEMP             ADC_CHANNEL_5
#define ADC_BANK2_CHANNEL_R_SHK_DISP            ADC_CHANNEL_6
#define ADC_BANK2_CHANNEL_PU2                   ADC_CHANNEL_15
#define ADC_BANK2_CHANNEL_BR_PR                 ADC_CHANNEL_8
#define ADC_BANK2_CHANNEL_SPARE3                ADC_CHANNEL_12
#define ADC_BANK2_CHANNEL_SPARE4                ADC_CHANNEL_13
#define ADC_BANK2_CHANNEL_APPS_P2               ADC_CHANNEL_2
#define ADC_BANK2_CHANNEL_BOARD_TEMP            ADC_CHANNEL_9

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
    hadc1.Init.NbrOfConversion       = 8;
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
    sConfig.Channel      = ADC_BANK1_CHANNEL_R_BR_TEMP;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK1_CHANNEL_L_SHK_DISP;
    sConfig.Rank         = ADC_REGULAR_RANK_2;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK1_CHANNEL_PU1;
    sConfig.Rank         = ADC_REGULAR_RANK_3;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK1_CHANNEL_BR_POT;
    sConfig.Rank         = ADC_REGULAR_RANK_4;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK1_CHANNEL_SPARE1;
    sConfig.Rank         = ADC_REGULAR_RANK_5;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK1_CHANNEL_SPARE2;
    sConfig.Rank         = ADC_REGULAR_RANK_6;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK1_CHANNEL_APPS_P1;
    sConfig.Rank         = ADC_REGULAR_RANK_7;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK1_CHANNEL_MCU_TEMP;
    sConfig.Rank         = ADC_REGULAR_RANK_8;
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
    hadc2.Init.NbrOfConversion       = 8;
    if (HAL_ADC_Init(&hadc2) != HAL_OK)
    {
        Error_Handler();
    }

    // Configure Regular Channels
    sConfig.Channel      = ADC_BANK2_CHANNEL_L_BR_TEMP;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK2_CHANNEL_R_SHK_DISP;
    sConfig.Rank         = ADC_REGULAR_RANK_2;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK2_CHANNEL_PU2;
    sConfig.Rank         = ADC_REGULAR_RANK_3;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK2_CHANNEL_BR_PR;
    sConfig.Rank         = ADC_REGULAR_RANK_4;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK2_CHANNEL_SPARE3;
    sConfig.Rank         = ADC_REGULAR_RANK_5;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK2_CHANNEL_SPARE4;
    sConfig.Rank         = ADC_REGULAR_RANK_6;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK2_CHANNEL_APPS_P2;
    sConfig.Rank         = ADC_REGULAR_RANK_7;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sConfig.Channel      = ADC_BANK2_CHANNEL_BOARD_TEMP;
    sConfig.Rank         = ADC_REGULAR_RANK_8;
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
