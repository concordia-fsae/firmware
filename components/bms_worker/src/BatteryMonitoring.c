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
#include "HW_adc.h"

#include "FreeRTOS.h"

/**< Other Includes */
#include "Module.h"
#include <stdint.h>

#include "CELL.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#ifndef BMS_CONFIGURED_CELLS
# define BMS_CONFIGURED_CELLS 4
#endif

#ifndef BMS_CONFIGURED_PARALLEL_CELLS
# define BMS_CONFIGURED_PARALLEL_CELLS 1
#endif

#ifndef BMS_CONFIGURED_SAMPLING_TIME_MS
# define BMS_CONFIGURED_SAMPLING_TIME_MS 20
#endif
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

void BMS_CalcSegStats(void);
void BMS_CheckError(void);


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

    if (BMS.state == BMS_SAMPLING)
    {
        for (uint16_t i = startIndex; i < endIndex; i++)
        {
            uint8_t channelIndex = i % ADC_CHANNEL_COUNT;
            BMS.data.cell_voltages[channelIndex].raw += BMS.data.adc_buffer[i];
            BMS.data.cell_voltages[channelIndex].count++;
        }

        if (half == BUFFER_HALF_LOWER)
            return;

        for (uint8_t i = 0; i < ADC_CHANNEL_COUNT; i++)
        {
            BMS.data.cell_voltages[i].value                         = (float32_t)BMS.data.cell_voltages[i].raw / BMS.data.cell_voltages[i].count;
            BMS.cell_voltages[max_chip.config.output.output.cell++] = HW_ADC_GetVFromCount(BMS.data.cell_voltages[i].value);
        }

        for (uint16_t i = startIndex; i < endIndex; i++)
        {
            uint8_t channelIndex                       = i % ADC_CHANNEL_COUNT;
            BMS.data.cell_voltages[channelIndex].raw   = 0;
            BMS.data.cell_voltages[channelIndex].count = 0;
        }

        if (max_chip.config.output.output.cell == CELL_COUNT || max_chip.config.output.output.cell == BMS_CONFIGURED_CELLS)
        {
            max_chip.config.output.output.cell   = CELL1;
            max_chip.config.output.state         = PACK_VOLTAGE;
            max_chip.config.sampling_start_100us = UINT32_MAX;

            BMS.state = BMS_PACK_SAMPLING;

            MAX_ReadWriteToChip();

            return;
        }

        BMS.data.measurement_complete = true;
    }
    else if (BMS.state == BMS_PACK_SAMPLING)
    {
        for (uint16_t i = startIndex; i < endIndex; i++)
        {
            BMS.data.pack_voltage.raw += BMS.data.adc_buffer[i];
            BMS.data.pack_voltage.count++;
        }

        if (half == BUFFER_HALF_LOWER)
            return;

        BMS.data.pack_voltage.value = (float32_t)BMS.data.pack_voltage.raw / BMS.data.pack_voltage.count;
        BMS.pack_voltage            = (16 * (uint32_t)HW_ADC_GetVFromCount(BMS.data.pack_voltage.value)) / 10;

        BMS.data.pack_voltage.raw     = 0;
        BMS.data.pack_voltage.count   = 0;
        BMS.data.measurement_complete = true;
    }
}

/**
 * @brief  BMS Module init function
 */
static void BMS_Init()
{
    BMS.state = BMS_INIT;

    if (!MAX_Init())
    {
        BMS.state = BMS_ERROR;
    }

    if (!HW_ADC_Calibrate(&hadc2))
    {
        BMS.state = BMS_ERROR;
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
    static enum {
        SETTLING = 0x00,
        START,
        WAITING,
    } state = SETTLING;

    if (BMS.state == BMS_SAMPLING &&
        ((max_chip.config.sampling_start_100us + pdMS_TO_TICKS(BMS_CONFIGURED_SAMPLING_TIME_MS)) < HW_GetTick()))
    {
        if (state == SETTLING)
        {
            max_chip.config.sampling           = false;
            max_chip.config.diagnostic_enabled = false;
            max_chip.config.output.state       = CELL_VOLTAGE;

            MAX_ReadWriteToChip();
            BMS.data.measurement_complete = false;
            state                         = START;
        }
        else if (state == START)
        {
            // HW_ADC_Start_DMA(&hadc2, (uint32_t*)&BMS.data.adc_buffer, BMS_ADC_BUF_LEN);
            if (HW_ADC_Request_DMA(ADC_REQUEST_BMS, (uint32_t*)&BMS.data.adc_buffer))
                state = WAITING;
        }
        else if (state == WAITING && BMS.data.measurement_complete)
        {
            state = SETTLING;
        }
    }
    else if (BMS.state == BMS_PACK_SAMPLING)
    {
        if (state == WAITING)
        {
            state = START;
        }
        else if (state == START)
        {
            BMS.data.measurement_complete = false;
            if (HW_ADC_Request_DMA(ADC_REQUEST_BMS, (uint32_t*)&BMS.data.adc_buffer))
                state = SETTLING;
        }
        else if (state == SETTLING && BMS.data.measurement_complete)
        {
            BMS.state = BMS_WAITING;
        }
    }
}

/**
 * @brief  10 Hz BMS periodic function
 */
static void BMS10Hz_PRD()
{
    if (BMS.state == BMS_INIT)
    {
        static uint32_t start_time = 0;

        MAX_ReadWriteToChip();

        if (!max_chip.state.ready)
        {
            return;
        }
        else if (start_time == 0)
        {
            max_chip.config.sampling           = true;
            max_chip.config.diagnostic_enabled = true;
            max_chip.config.output.state       = PACK_VOLTAGE;

            MAX_ReadWriteToChip();
            start_time = HW_GetTick();
            return;
        }
        else if (start_time + 1000 > HW_GetTick())
        {
            return; /**< wait atleast 100ms to sample the voltages for the first time */
        }

        max_chip.config.sampling           = false;
        max_chip.config.diagnostic_enabled = false;

        MAX_ReadWriteToChip();
        MAX_ReadWriteToChip(); /**< Re-read to get updated undervoltage information */

        for (uint8_t i = 0; i < BMS_CONFIGURED_CELLS; i++)
        {
            if ((max_chip.state.cell_undervoltage & (1 << i)) == 0x00)
                max_chip.state.connected_cells++;
        }

        if (max_chip.state.connected_cells == 0)
        {
            start_time = 0;
            return;
        }

        for (uint8_t i = 0; i < max_chip.state.connected_cells; i++)
        {
            /**< Check if any cell between first and last populated cells in the stack are disconnected*/
            if (max_chip.state.cell_undervoltage & (1 << i))
            {
                BMS.state = BMS_ERROR;
            }
        }

        BMS.state = BMS_SAMPLING;
    }
    else if (BMS.state == BMS_SAMPLING)
    {
        if (max_chip.config.sampling_start_100us == UINT32_MAX)
        {
            max_chip.config.sampling           = true;
            max_chip.config.diagnostic_enabled = false;
            max_chip.config.low_power_mode     = false;
            max_chip.config.balancing          = 0x00;
            max_chip.config.output.state       = PACK_VOLTAGE;
            max_chip.config.output.output.cell = CELL1; /**< Prepare for next step */

            MAX_ReadWriteToChip();
            max_chip.config.sampling_start_100us = HW_GetTick();
            BMS.data.measurement_complete        = false;
            return;
        }
    }
    else if (BMS.state == BMS_WAITING)
    {
        BMS.state                            = BMS_SAMPLING;
        max_chip.config.sampling_start_100us = UINT32_MAX;
        BMS_CalcSegStats();
        BMS_CheckError(); /**< If cells are in error, it will override from sampling state */
    }
    else if (BMS.state == BMS_DIAGNOSTIC)
    {
        if (max_chip.config.sampling_start_100us + pdMS_TO_TICKS(BMS_CONFIGURED_SAMPLING_TIME_MS * 10) < HW_GetTick())
        {
            max_chip.config.sampling             = false;
            max_chip.config.diagnostic_enabled   = false;
            max_chip.config.low_power_mode       = false;
            max_chip.config.balancing            = 0x00;
            max_chip.config.output.state         = PACK_VOLTAGE;
            max_chip.config.output.output.cell   = CELL1; /**< Prepare for next step */
            max_chip.config.sampling_start_100us = UINT32_MAX;
            MAX_ReadWriteToChip();

            BMS.state = BMS_SAMPLING;
        }
    }
    else if (BMS.state == BMS_CALIBRATING)
    {
        MAX_ReadWriteToChip();

        if (max_chip.state.ready)
            BMS.state = BMS_WAITING;
    }
}

/**
 * @brief  1Hz BMS periodic function
 */
static void BMS1Hz_PRD()
{
    if (BMS.state == BMS_WAITING)
    {
        static uint8_t cnt = 0;

        if (cnt++ % 2 == 0)
        {
            max_chip.config.diagnostic_enabled = false;
            max_chip.config.sampling           = false;
            max_chip.config.low_power_mode     = false;
            max_chip.config.balancing          = 0x00;
            max_chip.config.output.state       = AMPLIFIER_SELF_CALIBRATION;
            max_chip.config.output.output.cell = CELL1; /**< Prepare for next step */
            MAX_ReadWriteToChip();

            max_chip.config.output.state       = PACK_VOLTAGE;
            max_chip.config.output.output.cell = CELL1; /**< Prepare for next step */
            MAX_ReadWriteToChip();

            BMS.state = BMS_CALIBRATING;
            return;
        }

        BMS.state = BMS_DIAGNOSTIC;

        max_chip.config.diagnostic_enabled = true;
        max_chip.config.sampling           = true;
        max_chip.config.low_power_mode     = false;
        max_chip.config.balancing          = 0x00;
        max_chip.config.output.state       = PACK_VOLTAGE;
        max_chip.config.output.output.cell = CELL1; /**< Prepare for next step */
        MAX_ReadWriteToChip();

        max_chip.config.sampling_start_100us = HW_GetTick();
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

/**
 * @brief  Calculates the segments statistics.
 */
void BMS_CalcSegStats(void)
{
    float32_t batt_tmp = 0;

    BMS.max_voltage             = 0x00;
    BMS.min_voltage             = UINT16_MAX;
    BMS.avg_voltage             = 0x00;
    BMS.min_capacity            = UINT16_MAX;
    BMS.max_capacity            = 0x00;
    BMS.avg_capacity            = 0x00;
    BMS.calculated_pack_voltage = 0x00;

    for (uint8_t i = 0; i < max_chip.state.connected_cells; i++)
    {
        BMS.max_voltage = (BMS.max_voltage > BMS.cell_voltages[i]) ? BMS.max_voltage : BMS.cell_voltages[i];
        BMS.min_voltage = (BMS.min_voltage < BMS.cell_voltages[i]) ? BMS.min_voltage : BMS.cell_voltages[i];
        batt_tmp += BMS.cell_voltages[i];
        BMS.calculated_pack_voltage += BMS.cell_voltages[i] / 10;
    }

    BMS.avg_voltage = batt_tmp / max_chip.state.connected_cells;

    BMS.min_capacity = BMS_CONFIGURED_PARALLEL_CELLS * CELL_GetCapacityfromV(BMS.min_voltage);
    BMS.max_capacity = BMS_CONFIGURED_PARALLEL_CELLS * CELL_GetCapacityfromV(BMS.max_voltage);
    BMS.avg_capacity = BMS_CONFIGURED_PARALLEL_CELLS * CELL_GetCapacityfromV(BMS.avg_voltage);
}

/**
 * @brief  Checks for errors relative to the cells.
 */
void BMS_CheckError(void)
{
    for (uint8_t i = 0; i < max_chip.state.connected_cells; i++)
    {
        /**< Check if any cell between first and last populated cells in the stack are disconnected*/
        if (max_chip.state.cell_undervoltage & (1 << i))
        {
            BMS.state = BMS_ERROR;
        }
    }
}