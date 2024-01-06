/**
 * @file BatteryMonitoring.c
 * @brief  Source code for Battery Monitoring Application
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-27
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module header */
#include "BatteryMonitoring.h"

/**< Driver Includes */
#include "HW.h"

/**< Other Includes */
#include "Module.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern MAX14921_S max_chip;


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/**
 * @brief  Stores public BMS struct
 */
BMS_S BMS;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Unpack's the BMS ADC's DMA buffer
 *
 * @param bufferHalf_E Upper or lower half of buffer
 */
void BMS_UnpackADCBuffer(bufferHalf_E half)
{
    uint16_t startIndex = (half == BUFFER_HALF_LOWER) ? 0U : BMS_ADC_BUF_LEN / 2U;
    uint16_t endIndex   = startIndex + (BMS_ADC_BUF_LEN / 2U);

    for (uint16_t i = startIndex; i < endIndex; i++)
    {
        uint8_t channelIndex = i % ADC_CHANNEL_COUNT;
        BMS.data.cell_voltages[channelIndex].raw += BMS.data.adc_buffer[i];
        BMS.data.cell_voltages[channelIndex].count++;
    }

    if (half == BUFFER_HALF_LOWER) return;

    for (uint8_t i = 0; i < ADC_CHANNEL_COUNT; i++)
    {
        BMS.data.cell_voltages[i].value = (float32_t) BMS.data.cell_voltages[i].raw / BMS.data.cell_voltages[i].count;
        BMS.cell_voltages[max_chip.config.output.output.cell++] = BMS.data.cell_voltages[i].value * 1000;
        BMS.data.sampling_started = false;
    }

    if (max_chip.config.output.output.cell == CELL_COUNT)
    {
        max_chip.config.output.output.cell = CELL1;

        if (BMS.state == BMS_MEASURING)
        {
            BMS.state = BMS_WAITING;
        }
        else {
            BMS.state = BMS_BALANCING;
        }
    }
}

/**
 * @brief  BMS Module init function
 */
static void BMS_Init()
{
    BMS.state = BMS_INIT;

    if (!HW_ADC_Calibrate(&hadc1))
    {
        BMS.state = BMS_ERROR;
    }

    if (!MAX_Init())
    {
        BMS.state = BMS_ERROR;
    }
    
    if (BMS.state != BMS_ERROR){
        BMS.state = BMS_WAITING;
    }
//    MAX_Init();
//
//    max_chip.config.sampling = true;
//    max_chip.config.output.state = PACK_VOLTAGE;
//    max_chip.config.output.output.cell = CELL1;
//    max_chip.config.diagnostic_enabled = true;
//    MAX_ReadWriteToChip();
//    HAL_Delay(10000);
//    max_chip.config.sampling = false;
//    max_chip.config.output.state = PACK_VOLTAGE;
//    max_chip.config.output.output.cell = CELL1;
//    max_chip.config.diagnostic_enabled = true;
//    MAX_ReadWriteToChip();
//
//    while (1)
//    {
//        max_chip.config.output.state = PACK_VOLTAGE;
//        max_chip.config.output.output.cell = CELL1;
//        max_chip.config.sampling = true;
//        max_chip.config.diagnostic_enabled = false;
//        MAX_ReadWriteToChip();
//        MAX_ReadWriteToChip();
//        HAL_Delay(1000);
//        max_chip.config.sampling = false;
//        max_chip.config.diagnostic_enabled = false;
//        MAX_ReadWriteToChip();
//        MAX_ReadWriteToChip();
//        HAL_Delay(5);
//        max_chip.config.output.state = CELL_VOLTAGE;
//        max_chip.config.output.output.cell = CELL1;
//        max_chip.config.sampling = false;
//        max_chip.config.diagnostic_enabled = false;
//        MAX_ReadWriteToChip();
//        MAX_ReadWriteToChip();
//        HAL_Delay(5);
//        max_chip.config.output.state = CELL_VOLTAGE;
//        max_chip.config.output.output.cell = CELL2;
//        max_chip.config.sampling = false;
//        max_chip.config.diagnostic_enabled = false;
//        MAX_ReadWriteToChip();
//        MAX_ReadWriteToChip();
//        HAL_Delay(5);
//        max_chip.config.output.state = CELL_VOLTAGE;
//        max_chip.config.output.output.cell = CELL3;
//        max_chip.config.sampling = false;
//        max_chip.config.diagnostic_enabled = false;
//        MAX_ReadWriteToChip();
//        MAX_ReadWriteToChip();
//        HAL_Delay(5);
//        max_chip.config.output.state = CELL_VOLTAGE;
//        max_chip.config.output.output.cell = CELL4;
//        max_chip.config.sampling = false;
//        max_chip.config.diagnostic_enabled = false;
//        MAX_ReadWriteToChip();
//        MAX_ReadWriteToChip();
//        HAL_Delay(5);
//        max_chip.config.output.state = PACK_VOLTAGE;
//        max_chip.config.output.output.cell = CELL1;
//        max_chip.config.sampling = false;
//        max_chip.config.diagnostic_enabled = false;
//        MAX_ReadWriteToChip();
//        MAX_ReadWriteToChip();
//        HAL_Delay(5);
//    }

}

/**
 * @brief  10kHz BMS periodic function. Used for voltage sampling
 */
static void BMS10kHz_PRD()
{
//    if (BMS.state == BMS_SAMPLING)
//    {
//        if (BMS.data.sampling_started == false)
//        {
//            max_chip.config.sampling = true;
//
//            //MAX_ReadWriteToChip();
//        }
//        else {
//            max_chip.config.sampling = false;
//            max_chip.config.output.output.cell = CELL1;
//
//            HW_ADC_Start_DMA(&hadc1, (uint32_t*)&BMS.data.adc_buffer, BMS_ADC_BUF_LEN);
//        }
//    }
}

/**
 * @brief  10 Hz BMS periodic function
 */
static void BMS10Hz_PRD()
{
}

/**
 * @brief  1Hz BMS periodic function
 */
static void BMS1Hz_PRD()
{
    static uint8_t cnt = 0;

    if (cnt++ == 10)
    {
        cnt = 0;

        BMS.state = BMS_DIAGNOSTIC;
    }
}

/**
 * @brief  BMS Module descriptor
 */
const ModuleDesc_S BMS_desc = {
    .moduleInit        = &BMS_Init,
    .periodic10kHz_CLK = &BMS10kHz_PRD,
    .periodic10Hz_CLK  = &BMS10Hz_PRD,
    .periodic1Hz_CLK   = &BMS1Hz_PRD,
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/
