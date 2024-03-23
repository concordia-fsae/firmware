/**
 * @file BatteryMonitoring.c
 * @brief  Source code for Battery Monitoring Application
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
#include "string.h"
#include <stdint.h>

#include "CELL.h"
#include "IO.h"

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

#ifndef STANDARD_CHARGE_CURRENT
#define STANDARD_CHARGE_CURRENT 4.2
#endif

#ifndef MAX_CONTINOUS_DISCHARGE_CURRENT
#define MAX_CONTINOUS_DISCHARGE_CURRENT 45 
#endif
/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern MAX_S max_chip;


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

BMS_S BMS;


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void BMS_calcSegStats(void);
void BMS_checkError(void);


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  BMS Module init function
 */
static void BMS_Init(void)
{
    memset(&BMS, 0, sizeof(BMS));
    BMS.state = BMS_INIT;

    if (!MAX_init())
    {
        BMS.state = BMS_ERROR;
    }
}

/**
 * @brief  10 Hz BMS periodic function
 */
static void BMS1kHz_PRD()
{
    if (BMS.state == BMS_PARASITIC)
    {
        if (max_chip.config.sampling_start_100us == UINT32_MAX)
        {
            max_chip.config.sampling           = false;
            max_chip.config.diagnostic_enabled = false;
            max_chip.config.low_power_mode     = false;
            max_chip.config.balancing          = 0x00;
            max_chip.config.output.state       = MAX_PARASITIC_ERROR_CALIBRATION;

            MAX_readWriteToChip();
            max_chip.config.sampling_start_100us = HW_getTick();
            return;
        }
        else if (HW_getTick() >= max_chip.config.sampling_start_100us + pdMS_TO_TICKS(BMS_CONFIGURED_SAMPLING_TIME_MS))
        {
            max_chip.config.sampling_start_100us = UINT32_MAX;
            BMS.state                            = BMS_PARASITIC_MEASUREMENT;
            BMS.pack_voltage                     = IO.segment * 1000 * 16;
        }
    }
    else if (BMS.state == BMS_SAMPLING)
    {
        if (max_chip.config.sampling_start_100us == UINT32_MAX)
        {
            max_chip.config.sampling           = true;
            max_chip.config.diagnostic_enabled = false;
            max_chip.config.low_power_mode     = false;
            max_chip.config.balancing          = 0x00;
            max_chip.config.output.state       = MAX_PACK_VOLTAGE;
            max_chip.config.output.output.cell = MAX_CELL1; /**< Prepare for next step */

            MAX_readWriteToChip();
            max_chip.config.sampling_start_100us = HW_getTick();
            return;
        }
        else if (HW_getTick() >= max_chip.config.sampling_start_100us + pdMS_TO_TICKS(BMS_CONFIGURED_SAMPLING_TIME_MS))
        {
            max_chip.config.sampling_start_100us = UINT32_MAX;
            BMS.state                            = BMS_HOLDING;
            BMS.pack_voltage                     = IO.segment * 1000 * 16;
        }
    }
    else if (BMS.state == BMS_WAITING)
    {
        BMS.state                            = BMS_SAMPLING;
        max_chip.config.sampling_start_100us = UINT32_MAX;
        BMS_calcSegStats();
        BMS_checkError(); /**< If cells are in error, it will override from sampling state */
    }
    else if (BMS.state == BMS_DIAGNOSTIC)
    {
        if (max_chip.config.sampling_start_100us + pdMS_TO_TICKS(BMS_CONFIGURED_SAMPLING_TIME_MS * 10) < HW_getTick())
        {
            max_chip.config.sampling             = false;
            max_chip.config.diagnostic_enabled   = false;
            max_chip.config.low_power_mode       = false;
            max_chip.config.balancing            = 0x00;
            max_chip.config.output.state         = MAX_PACK_VOLTAGE;
            max_chip.config.output.output.cell   = MAX_CELL1; /**< Prepare for next step */
            max_chip.config.sampling_start_100us = UINT32_MAX;
            MAX_readWriteToChip();

            BMS.state = BMS_SAMPLING;
        }
    }
    else if (BMS.state == BMS_CALIBRATING)
    {
        MAX_readWriteToChip();

        if (max_chip.state.ready)
            BMS.state = BMS_WAITING;
    }
}

/**
 * @brief  1Hz BMS periodic function
 */
static void BMS1Hz_PRD()
{
    if (BMS.state == BMS_INIT)
    {
        static uint32_t start_time = 0;

        MAX_readWriteToChip();

        if (!max_chip.state.ready)
        {
            return;
        }
        else if (start_time == 0)
        {
            max_chip.config.sampling           = true;
            max_chip.config.diagnostic_enabled = true;
            max_chip.config.output.state       = MAX_PACK_VOLTAGE;

            MAX_readWriteToChip();
            start_time = HW_getTick();
            return;
        }
        else if (start_time + 1000 > HW_getTick())
        {
            return; /**< wait atleast 100ms to sample the voltages for the first time */
        }

        max_chip.config.sampling           = false;
        max_chip.config.diagnostic_enabled = false;

        MAX_readWriteToChip();
        MAX_readWriteToChip(); /**< Re-read to get updated undervoltage information */

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
                BMS.state          = BMS_ERROR;
                BMS.cells[i].state = BMS_CELL_ERROR;
            }
            else
            {
                BMS.cells[i].state = BMS_CELL_CONNECTED;
            }
        }

        BMS.connected_cells = max_chip.state.connected_cells;

        BMS.state = BMS_PARASITIC;
        return;
    }

    static uint8_t cnt = 0;

    if (cnt++ % 2 == 0)
    {
        max_chip.config.diagnostic_enabled = false;
        max_chip.config.sampling           = false;
        max_chip.config.low_power_mode     = false;
        max_chip.config.balancing          = 0x00;
        max_chip.config.output.state       = MAX_AMPLIFIER_SELF_CALIBRATION;
        max_chip.config.output.output.cell = MAX_CELL1; /**< Prepare for next step */
        MAX_readWriteToChip();

        max_chip.config.output.state       = MAX_PACK_VOLTAGE;
        max_chip.config.output.output.cell = MAX_CELL1; /**< Prepare for next step */
        MAX_readWriteToChip();

        BMS.state = BMS_CALIBRATING;
    }

    while (BMS.state == BMS_CALIBRATING)
        ;

    max_chip.config.diagnostic_enabled = true;
    max_chip.config.sampling           = true;
    max_chip.config.low_power_mode     = false;
    max_chip.config.balancing          = 0x00;
    max_chip.config.output.state       = MAX_PACK_VOLTAGE;
    max_chip.config.output.output.cell = MAX_CELL1; /**< Prepare for next step */
    MAX_readWriteToChip();

    max_chip.config.sampling_start_100us = HW_getTick();
    BMS.state                            = BMS_DIAGNOSTIC;
}

/**
 * @brief  BMS Module descriptor
 */
const ModuleDesc_S BMS_desc = {
    .moduleInit       = &BMS_Init,
    .periodic1kHz_CLK = &BMS1kHz_PRD,
    .periodic1Hz_CLK  = &BMS1Hz_PRD,
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Calculates the segments statistics.
 */
void BMS_calcSegStats(void)
{
    

    // initiate the array (room for 8 segments and 16 cells, only 7 and 14 used respectively)
    uint16_t cell_total_capacity[8][16] = {{20924,20921,20920,20910,20909,20902,20901,20899,20899,20899,20899,20892,20889,20883,0    ,0}
                                          ,{20876,20874,20872,20869,20865,20864,20863,20858,20858,20857,20855,20854,20854,20852,0    ,0}
                                          ,{20851,20851,20850,20849,20848,20848,20847,20847,20847,20846,20846,20846,20845,20845,0    ,0}
                                          ,{20845,20844,20844,20844,20843,20843,20843,20842,20842,20841,20841,20841,20840,20840,0    ,0}
                                          ,{20840,20839,20839,20839,20839,20838,20837,20837,20837,20835,20834,20834,20833,20833,0    ,0}
                                          ,{20832,20831,20830,20830,20828,20827,20826,20825,20822,20819,20817,20816,20815,20815,0    ,0}
                                          ,{20810,20810,20808,20807,20805,20801,20800,20798,20794,20793,20787,20779,20779,20778,0    ,0}
                                          ,{0    ,0    ,0    ,0    ,0    ,0    ,0    ,0    ,0    ,0    ,0    ,0    ,0    ,0    ,0    ,0}
                                          };

    for (uint8_t i = 0; i < BMS.connected_cells; i++)
    {
        BMS.cells[i].voltage = IO.cell[i] * 10000 - BMS.cells[i].parasitic_corr;
        if (BMS.cells[i].voltage > 25000 && BMS.cells[i].voltage < 42000)
        {
            BMS.cells[i].capacity     = cell_total_capacity[IO.addr][i] * CELL_getSoCfromV(IO.cell[i]);
            BMS.cells[i].relative_SoC = CELL_getSoCfromV(BMS.cells[i].voltage);
            BMS.cells[i].state        = BMS_CELL_CONNECTED;
        }
        else
        {
            BMS.cells[i].capacity = 0;
            BMS.cells[i].state    = BMS_CELL_ERROR;
        }
    }

    float32_t batt_tmp = 0;

    BMS.voltage.max              = 0x00;
    BMS.voltage.min              = UINT16_MAX;
    BMS.voltage.avg              = 0x00;
    BMS.capacity.min             = UINT16_MAX;
    BMS.capacity.max             = 0x00;
    BMS.capacity.avg             = 0x00;
    BMS.calculated_pack_voltage  = 0x00;
    BMS.relative_SoC.max         = UINT8_MAX;
    BMS.relative_SoC.min         = 0x00;
    BMS.relative_SoC.avg         = 0x00;

    for (uint8_t i = 0; i < max_chip.state.connected_cells; i++)
    {
        BMS.voltage.max = (BMS.voltage.max > BMS.cells[i].voltage) ? BMS.voltage.max : BMS.cells[i].voltage;
        BMS.voltage.min = (BMS.voltage.min < BMS.cells[i].voltage) ? BMS.voltage.min : BMS.cells[i].voltage;
        BMS.capacity.max = (BMS.capacity.max > BMS.cells[i].capacity) ? BMS.capacity.max : BMS.cells[i].capacity;
        BMS.capacity.min = (BMS.capacity.min < BMS.cells[i].capacity) ? BMS.capacity.min : BMS.cells[i].capacity; 
        batt_tmp += BMS.cells[i].voltage;
        BMS.calculated_pack_voltage += BMS.cells[i].voltage / 10;
    }

    BMS.voltage.avg     = batt_tmp / max_chip.state.connected_cells;

    BMS.relative_SoC.min = CELL_getSoCfromV(BMS.voltage.min);
    BMS.relative_SoC.max = CELL_getSoCfromV(BMS.voltage.max);
    BMS.relative_SoC.avg = CELL_getSoCfromV(BMS.voltage.avg);

    BMS.discharge_limit  = (BMS_dischargeLimit_SoC(BMS.relative_SoC.min) + BMS_dischargeLimit_heat(ENV.values.max_temp) > MAX_CONTINOUS_DISCHARGE_CURRENT) ? 0 : BMS_dischargeLimit_SoC(BMS.relative_SoC.min) + BMS_dischargeLimit_heat(ENV.values.max_temp);
    BMS.charge_limit     = (BMS_chargeLimit_SoC(BMS.relative_SoC.min)    + BMS_chargeLimit_heat(ENV.values.max_temp)    > STANDARD_CHARGE_CURRENT)         ? 0 : BMS_chargeLimit_SoC(BMS.relative_SoC.min)    + BMS_chargeLimit_heat(ENV.values.max_temp);
}

/**
 * @brief  Checks for errors relative to the cells.
 */
void BMS_checkError(void)
{
    for (uint8_t i = 0; i < BMS.connected_cells; i++)
    {
        /**< Check if any cell between first and last populated cells in the stack are disconnected*/
        if (BMS.cells[i].state != BMS_CELL_CONNECTED)
        {
            BMS.state = BMS_ERROR;
        }
    }
}

/**
 * @brief  Set output cell of the cell multiplexer
 *
 * @param cell Voltage of cell to output
 */
void BMS_setOutputCell(MAX_selectedCell_E cell)
{
    max_chip.config.sampling           = false;
    max_chip.config.diagnostic_enabled = false;
    max_chip.config.low_power_mode     = false;
    max_chip.config.balancing          = 0x00;
    max_chip.config.output.state       = MAX_CELL_VOLTAGE;
    max_chip.config.output.output.cell = cell;
    MAX_readWriteToChip();

    HW_usDelay(15U);
}

/**
 * @brief Callback function for measurement completion
 */
void BMS_measurementComplete(void)
{
    if (BMS.state == BMS_HOLDING)
    {
        BMS.state = BMS_WAITING;
    }
    else if (BMS.state == BMS_PARASITIC_MEASUREMENT)
    {
        for (uint8_t i = 0; i < BMS.connected_cells; i++)
        {
            BMS.cells[i].parasitic_corr = ((uint32_t)IO.cell[i] * 10000) / 128;
            BMS.state                   = BMS_WAITING;
        }
    }
}

/**
 * @brief Increases/decreases delta charge limit
 *
 * @param relativeSoC state of charge of a cell
 * 
 * @retval returns amperage for group of 5 cells (so need to multiply by 5)
 */
uint8_t BMS_chargeLimit_SoC(uint8_t relative_SoC) {
    if (relative_SoC <= 80){
        return STANDARD_CHARGE_CURRENT;
    } else {
        return (STANDARD_CHARGE_CURRENT-(-0.21f * relative_SoC + 21) * 5); 
    }
} 

/**
 * @brief Increases/decreases delta discharge limit 
 *
 * @param relativeSoC state of charge of a cell
 * 
 * @retval returns amperage for group of 5 cells (so need to multiply by 5)
 */
uint8_t BMS_dischargeLimit_SoC(uint8_t relative_SoC) { 
    if (relative_SoC > 20) {
        return MAX_CONTINOUS_DISCHARGE_CURRENT * 5;
    } else {
        return (MAX_CONTINOUS_DISCHARGE_CURRENT-(-2.25f*relative_SoC + 225)) * 5; 
    }
}

/**
 * @brief Increases/decreases delta discharge limit
 *
 * @param cell_temp temperature of a cell, stored in 0.1°C
 * 
 * @retval returns amperage for group of 5 cells (so need to multiply by 5)
 */
uint8_t BMS_dischargeLimit_heat(int16_t cell_temp) {
    if (cell_temp/10.0f > 60) {
        return 0;
    } else if (cell_temp/10.0f >= 48) {
        return (MAX_CONTINOUS_DISCHARGE_CURRENT-(-3.75f * cell_temp/10.0f + 225)) * 5; 
    } else {
        return MAX_CONTINOUS_DISCHARGE_CURRENT * 5;
    }
}

/**
 * @brief Increases/decreases delta charge limit
 *
 * @param cell_temp temperature of a cell, stored in 0.1°C
 * 
 * @retval returns amperage for group of 5 cells in parallel (so need to multiply by 5)
 */
uint8_t BMS_chargeLimit_heat(int16_t cell_temp) {
    if (cell_temp > 600) {
        return 0;
    } else if (cell_temp >= 480) {
        return (STANDARD_CHARGE_CURRENT-(-0.35f * cell_temp/10.0f + 21)) * 5;
    } else {
        return STANDARD_CHARGE_CURRENT * 5;
    }
}