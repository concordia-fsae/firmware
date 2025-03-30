/**
 * @file HW_adc.c
 * @brief  Source code for ADC firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW_adc_private.h"

// System Includes
#include "HW.h"
#include "string.h"
#include "LIB_simpleFilter.h"

_Static_assert(HW_ADC_BUF_LEN % ADC_BANK1_CHANNEL_COUNT == 0, "ADC Buffer Length should be a multiple of the number of ADC channels");
_Static_assert((HW_ADC_BUF_LEN / 2) % ADC_BANK1_CHANNEL_COUNT == 0, "ADC Buffer Length divided by two should be a multiple of the number of ADC channels");
_Static_assert(HW_ADC_BUF_LEN % ADC_BANK2_CHANNEL_COUNT == 0, "ADC Buffer Length should be a multiple of the number of ADC channels");
_Static_assert((HW_ADC_BUF_LEN / 2) % ADC_BANK2_CHANNEL_COUNT == 0, "ADC Buffer Length divided by two should be a multiple of the number of ADC channels");

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint32_t           adcBuffer[HW_ADC_BUF_LEN];
    LIB_simpleFilter_S adcData_bank1[ADC_BANK1_CHANNEL_COUNT];
    LIB_simpleFilter_S adcData_bank2[ADC_BANK2_CHANNEL_COUNT];
    float32_t          voltages1[ADC_BANK1_CHANNEL_COUNT];
    float32_t          voltages2[ADC_BANK2_CHANNEL_COUNT];
} inputs_S;

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static inputs_S inputs;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Get analog input voltage in V from ADC count
 * @param cnt ADC count
 * @retval unit:  V
 */
static float32_t HW_ADC_private_getVFromCount(uint16_t cnt)
{
    return ((float32_t)cnt) * (float32_t)(((float32_t)ADC_REF_VOLTAGE) / ((float32_t)ADC_MAX_COUNT));
}

/**
 * @brief  Unpack ADC buffer
 */
static void HW_ADC_private_unpackADCBuffer(void)
{
    for (uint8_t i = 0; i < ADC_BANK1_CHANNEL_COUNT; i++)
    {
        LIB_simpleFilter_clear(&inputs.adcData_bank1[i]);
    }
    for (uint8_t i = 0; i < ADC_BANK2_CHANNEL_COUNT; i++)
    {
        LIB_simpleFilter_clear(&inputs.adcData_bank2[i]);
    }

    for (uint16_t i = 0; i < HW_ADC_BUF_LEN; i++)
    {
        LIB_simpleFilter_increment(&inputs.adcData_bank1[i % ADC_BANK1_CHANNEL_COUNT], (inputs.adcBuffer[i] & 0xffff));
        LIB_simpleFilter_increment(&inputs.adcData_bank2[i % ADC_BANK2_CHANNEL_COUNT], (inputs.adcBuffer[i] >> 16U));
    }

    for (uint8_t i = 0; i < ADC_BANK1_CHANNEL_COUNT; i++)
    {
        LIB_simpleFilter_average(&inputs.adcData_bank1[i]);
        inputs.voltages1[i] = HW_ADC_private_getVFromCount((uint16_t)inputs.adcData_bank1[i].value);
    }
    for (uint8_t i = 0; i < ADC_BANK2_CHANNEL_COUNT; i++)
    {
        LIB_simpleFilter_average(&inputs.adcData_bank2[i]);
        inputs.voltages2[i] = HW_ADC_private_getVFromCount((uint16_t)inputs.adcData_bank2[i].value);
    }
}

/**
 * @brief Firmware functinputsn to initiate ADC calibraton.
 *
 * @param hadc Pointer to ADC peripheral
 *
 * @retval true = Success, false = Failure
 */
static HW_StatusTypeDef_E HW_ADC_calibrate(ADC_HandleTypeDef* hadc)
{
    return HAL_ADCEx_Calibration_Start(hadc) == HAL_OK ? HW_OK : HW_ERROR;
}

/**
 * @brief  Firmware functinputsn to start DMA transfer
 *
 * @param hadc Pointer to ADC peripheral
 * @param data Pointer to memory start address
 * @param size Size of buffer
 *
 * @retval true = Success, false = Failure
 */
static HW_StatusTypeDef_E HW_ADC_startDMA(ADC_HandleTypeDef* hadc, uint32_t* data, uint32_t size)
{
    return HAL_ADCEx_MultiModeStart_DMA(hadc, data, size) == HAL_OK ? HW_OK : HW_ERROR;
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

HW_StatusTypeDef_E HW_ADC_init(void)
{
    memset(&inputs, 0x00, sizeof(inputs));

    HW_StatusTypeDef_E status = HW_ADC_init_componentSpecific();
    HW_ADC_calibrate(&hadc1);
    HW_ADC_calibrate(&hadc2);
    HAL_ADC_Start(&hadc2);
    HW_ADC_startDMA(&hadc1, (uint32_t*)&inputs.adcBuffer, HW_ADC_BUF_LEN);

    return status;
}

/**
 * @brief  STM32 HAL callback. Called when DMA transfer is half complete.
 * @param hadc Pointer to ADC peripheral
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {}
}

/**
 * @brief  STM32 HAL callback. Called when DMA transfer is half complete.
 * @param hadc Pointer to ADC peripheral
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        HW_ADC_private_unpackADCBuffer();
    }
}

float32_t HW_ADC_getVFromBank1Channel(HW_adcChannels_bank1_E channel)
{
    return inputs.voltages1[channel];
}

float32_t HW_ADC_getVFromBank2Channel(HW_adcChannels_bank2_E channel)
{
    return inputs.voltages2[channel];
}
