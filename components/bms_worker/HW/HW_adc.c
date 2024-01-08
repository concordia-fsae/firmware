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

#include "string.h"

/**< Firmware Includes */
#include "HW_adc.h"

/**< Other Includes */
#include "BatteryMonitoring.h"
#include "IO.h"
#include "SystemConfig.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define ADC_PRECALIBRATION_DELAY_ADCCLOCKCYCLES 2U
#define ADC_CALIBRATION_TIMEOUT                 10U

#define ADC_MAX_COUNT 4095
#define ADC_REF_VOLTAGE 3.3
#define ADC_INPUT_VOLTAGE_DIVISOR 2

#define ADC_BUF_CNT IO_ADC_BUF_LEN
_Static_assert(IO_ADC_BUF_LEN == BMS_ADC_BUF_LEN, "BMS and IO must have same length ADC buffer for DMA.");

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc1;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static uint32_t  adc_buf[ADC_BUF_CNT]          = { 0 };
static uint32_t* adc_req_addr[ADC_REQUEST_CNT] = { 0 };
static uint32_t* adc_buf_addr[ADC_REQUEST_CNT] = { 0 };

static bool dma_running = false;


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void HW_ADC_UnpackBuffer(bufferHalf_E half);


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * MX_ADC1_Init
 */
void HW_ADC_Init(void)
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
    hadc1.Init.NbrOfConversion       = 1;
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

    HAL_ADC_Start(&hadc2);
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
        hdma_adc1.Init.Mode                = DMA_NORMAL;
        hdma_adc1.Init.Priority            = DMA_PRIORITY_MEDIUM;
        if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_LINKDMA(adcHandle, DMA_Handle, hdma_adc1);
        
        HAL_NVIC_SetPriority(ADC1_2_IRQn, DMA_IRQ_PRIO, 0);
        HAL_NVIC_EnableIRQ(ADC1_2_IRQn);
    }
    else if (adcHandle->Instance == ADC2)
    {
        // ADC1 clock enable
        __HAL_RCC_ADC2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitStruct.Pin  = CELL_VOLTAGE_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        HAL_GPIO_Init(CELL_VOLTAGE_Port, &GPIO_InitStruct);
        
        HAL_NVIC_SetPriority(ADC1_2_IRQn, DMA_IRQ_PRIO, 0);
        HAL_NVIC_EnableIRQ(ADC1_2_IRQn);
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
    }
}

// Called when first half of buffer is filled
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        HW_ADC_UnpackBuffer(BUFFER_HALF_LOWER);

        ADC_Request_E req = 0;

        for (; req < ADC_REQUEST_CNT; req++)
        {
            if (adc_buf_addr[req])
                break;
        }

        switch (req)
        {
            case ADC_REQUEST_IO:
                IO_UnpackAdcBuffer(BUFFER_HALF_LOWER);
                break;
            case ADC_REQUEST_BMS:
                BMS_UnpackADCBuffer(BUFFER_HALF_LOWER);
                break;
            default:
                break;
        }
    }
}

// Called when buffer is completely filled
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        HW_ADC_UnpackBuffer(BUFFER_HALF_UPPER);

        ADC_Request_E req = 0;

        for (; req < ADC_REQUEST_CNT; req++)
        {
            if (adc_buf_addr[req])
                break;
        }


        switch (req)
        {
            case ADC_REQUEST_IO:
                IO_UnpackAdcBuffer(BUFFER_HALF_UPPER);
                break;
            case ADC_REQUEST_BMS:
                BMS_UnpackADCBuffer(BUFFER_HALF_UPPER);
                break;
            default:
                return;
                break;
        }

        adc_buf_addr[ADC_REQUEST_IO] = 0;
        adc_buf_addr[ADC_REQUEST_BMS] = 0;

        if (adc_req_addr[(req + 1) % ADC_REQUEST_CNT])
        {
            adc_buf_addr[(req + 1) % ADC_REQUEST_CNT] = adc_req_addr[(req + 1) % ADC_REQUEST_CNT];
            adc_req_addr[(req + 1) % ADC_REQUEST_CNT] = 0;
            HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)&adc_buf, ADC_BUF_CNT);
            return;
        }
        else if (adc_req_addr[req])
        {
            adc_buf_addr[req] = adc_req_addr[req];
            adc_req_addr[req] = 0;
            HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)&adc_buf, ADC_BUF_CNT);
            return;
        }

        dma_running = false;
    }
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

bool HW_ADC_Calibrate(ADC_HandleTypeDef* hadc)
{
    return HAL_ADCEx_Calibration_Start(hadc) == HAL_OK;
}

bool HW_ADC_Start_DMA(ADC_HandleTypeDef* hadc, uint32_t* data, uint32_t size)
{
    return HAL_ADC_Start_DMA(hadc, data, size) == HAL_OK;
}

bool HW_ADC_Request_DMA(ADC_Request_E req, uint32_t* buf)
{
    if (!dma_running)
    {
        dma_running = true;
        HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)&adc_buf, ADC_BUF_CNT);
        adc_buf_addr[req] = buf;

        return true;
    }
    else if (adc_buf_addr[req] || adc_req_addr[req])
        return false;

    adc_req_addr[req] = buf;

    return true;
}

/**
 * @brief  Get analog input voltage in 0.1mV from ADC count 
 *
 * @param cnt ADC coun
 *
 * @retval   0.01mV
 */
uint16_t HW_ADC_GetVFromCount(uint16_t cnt)
{
    return ((uint32_t)cnt) * 10000 * ADC_INPUT_VOLTAGE_DIVISOR * ADC_REF_VOLTAGE / ADC_MAX_COUNT;
}


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

void HW_ADC_UnpackBuffer(bufferHalf_E half)
{
    ADC_Request_E req = 0;

    uint16_t startIndex = (half == BUFFER_HALF_LOWER) ? 0U : ADC_BUF_CNT / 2U;
    uint16_t endIndex   = startIndex + (ADC_BUF_CNT / 2U);

    for (; req < ADC_REQUEST_CNT; req++)
    {
        if (adc_buf_addr[req])
            break;
    }

    for (uint16_t i = startIndex; i < endIndex; i++)
    {
        *(adc_buf_addr[req] + i) = (req == ADC_REQUEST_IO) ? adc_buf[i] & (0xffff) : adc_buf[i] >> 16;
    }
}
