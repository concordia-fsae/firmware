/**
 * HW_adc.c
 * Hardware ADC implementation
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< FreeRTOS Includes */
#include "FreeRTOS.h"
#include "task.h"

/**< Firmware Includes */
#include "HW_adc.h"

/**< Other Includes */
#include "SystemConfig.h"
#include "IO.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ADC_PRECALIBRATION_DELAY_ADCCLOCKCYCLES    2U
#define ADC_CALIBRATION_TIMEOUT                    10U

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc1;
DMA_HandleTypeDef hdma_adc2;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * MX_ADC1_Init
 */
void HW_ADC_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = { 0 };

    // Cell Measurement config
    hadc1.Instance                   = ADC1;
    hadc1.Init.ScanConvMode          = ADC_SCAN_ENABLE;
    hadc1.Init.ContinuousConvMode    = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    // Configure Regular Channels

    sConfig.Channel      = ADC_CHANNEL_TEMPSENSOR;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
    
    // Common config
    hadc2.Instance                   = ADC2;
    hadc2.Init.ScanConvMode          = ADC_SCAN_ENABLE;
    hadc2.Init.ContinuousConvMode    = ENABLE;
    hadc2.Init.DiscontinuousConvMode = DISABLE;
    hadc2.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc2.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc2.Init.NbrOfConversion       = 1;
    if (HAL_ADC_Init(&hadc2) != HAL_OK)
    {
        Error_Handler();
    }
    
    sConfig.Channel      = ADC_CHANNEL_CELL_MEASUREMENT;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

void HW_ADC_DeInit()
{
    HAL_ADC_DeInit(&hadc2);
}

/**
 * HAL_ADC_MspInit
 * @param adcHandle adc handle to operate on
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
     } else if (adcHandle->Instance == ADC2)
     {
         // ADC1 clock enable
         __HAL_RCC_ADC2_CLK_ENABLE();
         __HAL_RCC_GPIOA_CLK_ENABLE();
         
         GPIO_InitStruct.Pin  = CELL_VOLTAGE_Pin;
         GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
         HAL_GPIO_Init(CELL_VOLTAGE_Port, &GPIO_InitStruct);
 
         hdma_adc2.Instance                 = DMA1_Channel2;
         hdma_adc2.Init.Direction           = DMA_PERIPH_TO_MEMORY;
         hdma_adc2.Init.PeriphInc           = DMA_PINC_DISABLE;
         hdma_adc2.Init.MemInc              = DMA_MINC_ENABLE;
         hdma_adc2.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
         hdma_adc2.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
         hdma_adc2.Init.Mode                = DMA_CIRCULAR;
         hdma_adc2.Init.Priority            = DMA_PRIORITY_MEDIUM;
         if (HAL_DMA_Init(&hdma_adc2) != HAL_OK)
         {
             Error_Handler();
         }
 
         __HAL_LINKDMA(adcHandle, DMA_Handle, hdma_adc2);
     }
}

/**
 * HAL_ADC_MspDeInit
 * @param adcHandle adc handle to operate on
 */
void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adcHandle)
{
     if (adcHandle->Instance == ADC1)
     {
         __HAL_RCC_ADC1_CLK_DISABLE();
 
         // ADC1 DMA DeInit
         HAL_DMA_DeInit(adcHandle->DMA_Handle);
     }
     if (adcHandle->Instance == ADC2)
     {
         // Peripheral clock disable
         __HAL_RCC_ADC2_CLK_DISABLE();
         
         HAL_GPIO_DeInit(CELL_VOLTAGE_Port, CELL_VOLTAGE_Pin);
 
         HAL_DMA_DeInit(adcHandle->DMA_Handle);
     }
}

// Called when first half of buffer is filled
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        IO_UnpackAdcBuffer(BUFFER_HALF_LOWER);
    }
    else if (hadc->Instance == ADC2)
    {
        
    }
}

// Called when buffer is completely filled
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        IO_UnpackAdcBuffer(BUFFER_HALF_UPPER);
    }
    else if (hadc->Instance == ADC2)
    {
        
    }
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

bool HW_ADC_Calibrate(ADC_HandleTypeDef *hadc)
{
    return HAL_ADCEx_Calibration_Start(hadc) == HAL_OK;
}

bool HW_ADC_Start_DMA(ADC_HandleTypeDef *hadc, uint32_t *data, uint32_t size)
{
    return HAL_ADC_Start_DMA(hadc, data, size) == HAL_OK;
}
