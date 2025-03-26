/**
 * @file HW_adc.c
 * @brief  Source code for ADC firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Inlcudes
#include "string.h"

// Firmware Includes
#include "HW_adc.h"

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
HW_StatusTypeDef_E HW_ADC_calibrate(ADC_HandleTypeDef* hadc)
{
    return HAL_ADCEx_Calibration_Start(hadc) == HAL_OK ? HW_OK : HW_ERROR;
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
HW_StatusTypeDef_E HW_ADC_startDMA(ADC_HandleTypeDef* hadc, uint32_t* data, uint32_t size)
{
    return HAL_ADCEx_MultiModeStart_DMA(hadc, data, size) == HAL_OK ? HW_OK : HW_ERROR;
}

/**
 * @brief  Get analog input voltage in 0.1mV from ADC count
 *
 * @param cnt ADC count
 *
 * @retval unit:  0.01mV
 */
float32_t HW_ADC_getVFromCount(uint16_t cnt)
{
    return ((float32_t)cnt) * ((float32_t)ADC_REF_VOLTAGE / (float32_t)ADC_MAX_COUNT);
}
