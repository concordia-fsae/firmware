/**
 * @file HW_adc.c
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
#include "HW_adc.h"

// Other Includes
#include "IO.h"
#include "SystemConfig.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ADC_PRECALIBRATION_DELAY_ADCCLOCKCYCLES 2U
#define ADC_CALIBRATION_TIMEOUT                 10U

#define ADC_MAX_COUNT 4095
#define ADC_REF_VOLTAGE 3.0F

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc1;


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void HW_ADC_unpackBuffer(bufferHalf_E half);


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Init function for ADC firmware
 */
void HW_ADC_init(void)
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
    hadc1.Init.NbrOfConversion = 2;
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

    sConfig.Channel      = ADC_N_CHANNEL;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    
    sConfig.Channel      = ADC_CHANNEL_TEMPSENSOR;
    sConfig.Rank         = ADC_REGULAR_RANK_2;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
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
    hadc2.Init.NbrOfConversion       = 2;
    if (HAL_ADC_Init(&hadc2) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.Channel      = ADC_P_CHANNEL;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    
    sConfig.Channel      = ADC_P_CHANNEL;
    sConfig.Rank         = ADC_REGULAR_RANK_2;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_ADC_Start(&hadc2);
}

/**
 * @brief  Callback for STM32 HAL once ADC initialization is complete
 *
 * @param adcHandle pointer to ADC peripheral
 */
void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle)
{
    UNUSED(adcHandle);
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    if (adcHandle->Instance == ADC1)
    {
        // ADC1 clock enable
        __HAL_RCC_ADC1_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();

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
        
        GPIO_InitStruct.Pin  = ADC_N_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        HAL_GPIO_Init(ADC_N_Port, &GPIO_InitStruct);
    }
    else if (adcHandle->Instance == ADC2)
    {
        // ADC1 clock enable
        __HAL_RCC_ADC2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitStruct.Pin  = ADC_P_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        HAL_GPIO_Init(ADC_P_Port, &GPIO_InitStruct);
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

        // ADC1 DMA DeInit
        HAL_GPIO_DeInit(ADC_N_Port, ADC_N_Pin);
        HAL_DMA_DeInit(adcHandle->DMA_Handle);
    }
    if (adcHandle->Instance == ADC2)
    {
        // Peripheral clock disable
        __HAL_RCC_ADC2_CLK_DISABLE();

        HAL_GPIO_DeInit(ADC_P_Port, ADC_P_Pin);
    }
}


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Firmware function to initiate ADC calibration.
 *
 * @param hadc Pointer to ADC peripheral
 *
 * @retval true = Success, false = Failure
 */
bool HW_ADC_calibrate(ADC_HandleTypeDef* hadc)
{
    return HAL_ADCEx_Calibration_Start(hadc) == HAL_OK;
}

/**
 * @brief  Firmware function to start DMA transfer
 *
 * @param hadc Pointer to ADC peripheral
 * @param data Pointer to memory start address
 * @param size Size of buffer
 *
 * @retval true = Success, false = Failure
 */
bool HW_ADC_startDMA(ADC_HandleTypeDef* hadc, uint32_t* data, uint32_t size)
{
    return HAL_ADCEx_MultiModeStart_DMA(hadc, data, size) == HAL_OK;
}
