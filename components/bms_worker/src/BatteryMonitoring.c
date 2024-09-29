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
#include "HW_tim.h"

#include "FreeRTOS.h"

/**< Other Includes */
#include "Module.h"
#include "string.h"
#include <stdint.h>

#include "CELL.h"
#include "IO.h"

#include "MessageUnpack_generated.h"
#include "FeatureDefines_generated.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define BMS_CONFIGURED_BALANCING_TIMEOUT 1100
#define BMS_CONFIGURED_BALANCING_MARGIN 0.050f // [V], precision 1mV
#define BMS_CONFIGURED_DERATING_DELAY 1000 // [ms]

#ifndef BMS_CONFIGURED_SAMPLING_TIME_MS
# define BMS_CONFIGURED_SAMPLING_TIME_MS 20
#endif

#ifndef BMS_CONFIGURED_BALANCING_TIME_MS
# define BMS_CONFIGURED_BALANCING_TIME_MS 500
#endif

#ifndef STANDARD_CHARGE_CURRENT
# define STANDARD_CHARGE_CURRENT 4.2f
#endif

#ifndef MAX_CONTINOUS_DISCHARGE_CURRENT
# define MAX_CONTINOUS_DISCHARGE_CURRENT 45
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
void BMS_checkFault(void);
void BMS_chargeLimit(void);
void BMS_dischargeLimit(void);


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
    if (BMS.state == BMS_SLEEPING)
    {
      return;
    }

    if (BMS.state == BMS_PARASITIC)
    {
        if (max_chip.config.sampling_start == UINT32_MAX)
        {
            max_chip.config.sampling           = false;
            max_chip.config.diagnostic_enabled = false;
            max_chip.config.low_power_mode     = false;
            max_chip.config.balancing          = 0x00;
            max_chip.config.output.state       = MAX_PARASITIC_ERROR_CALIBRATION;

            MAX_readWriteToChip();
            max_chip.config.sampling_start = HW_TIM_getTimeMS();
#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
            HW_TIM_10kHz_timerStart();
#endif // FEATUFEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
            return;
        }
        else if (HW_TIM_getTimeMS() >= max_chip.config.sampling_start + pdMS_TO_TICKS(BMS_CONFIGURED_SAMPLING_TIME_MS))
        {
            max_chip.config.sampling_start = UINT32_MAX;
            BMS.state                            = BMS_PARASITIC_MEASUREMENT;
            BMS.pack_voltage                     = IO.segment * 16;
#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
            HW_TIM_10kHz_timerStart();
#endif // FEATUFEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
        }
    }
    else if (BMS.state == BMS_SAMPLING)
    {
        if (max_chip.config.sampling_start == UINT32_MAX)
        {
            max_chip.config.sampling           = true;
            max_chip.config.low_power_mode     = false;
            max_chip.config.balancing          = 0x00;
            max_chip.config.output.state       = MAX_PACK_VOLTAGE;
            max_chip.config.output.output.cell = MAX_CELL1; /**< Prepare for next step */

            MAX_readWriteToChip();
            max_chip.config.sampling_start = HW_TIM_getTimeMS();
#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
            HW_TIM_10kHz_timerStart();
#endif // FEATUFEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
            return;
        }
        else if (HW_TIM_getTimeMS() >= max_chip.config.sampling_start + pdMS_TO_TICKS(BMS_CONFIGURED_SAMPLING_TIME_MS))
        {
            max_chip.config.sampling_start = UINT32_MAX;
            max_chip.config.diagnostic_enabled   = false;
            BMS.pack_voltage                     = IO.segment * 16;
            BMS_setOutputCell(BMS.connected_cells - 1);
            BMS.state                            = BMS_HOLDING;
#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
            HW_TIM_10kHz_timerStart();
#endif // FEATUFEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED

        }
    }
    else if (BMS.state == BMS_WAITING)
    {
        static uint8_t cnt = 0;

        if (cnt++ == 100)
        {
            return;
        }

        BMS_calcSegStats();
        cnt = 0;

        BMS.state                            = BMS_SAMPLING;
        max_chip.config.diagnostic_enabled   = false;
        max_chip.config.low_power_mode       = false;
        max_chip.config.sampling_start = UINT32_MAX;
    }
    else if (BMS.state == BMS_DIAGNOSTIC)
    {
        if (max_chip.config.sampling_start + pdMS_TO_TICKS(BMS_CONFIGURED_SAMPLING_TIME_MS) < HW_TIM_getTimeMS())
        {
            max_chip.config.sampling             = false;
            max_chip.config.diagnostic_enabled   = false;
            max_chip.config.low_power_mode       = false;
            max_chip.config.balancing            = 0x00;
            max_chip.config.output.state         = MAX_PACK_VOLTAGE;
            max_chip.config.output.output.cell   = MAX_CELL1; /**< Prepare for next step */
            max_chip.config.sampling_start = UINT32_MAX;
            MAX_readWriteToChip();

            BMS.state                          = BMS_SAMPLING;
        }
    }
    else if (BMS.state == BMS_CALIBRATING)
    {
        MAX_readWriteToChip();

        if (max_chip.state.ready)
        {
            BMS.state = BMS_WAITING;
        }
    }
}

/**
 * @brief  1Hz BMS periodic function
 */
static void BMS1Hz_PRD()
{
    if (BMS.state == BMS_SLEEPING)
    {
      return;
    }

    if (BMS.state == BMS_INIT)
    {
        static uint32_t start_time = 0;

        MAX_readWriteToChip();

        if (!max_chip.state.ready)
        {
            BMS.fault = true;
            return;
        }
        else if (start_time == 0)
        {
            max_chip.config.sampling           = true;
            max_chip.config.diagnostic_enabled = true;
            max_chip.config.output.state       = MAX_PACK_VOLTAGE;

            MAX_readWriteToChip();
            start_time = HW_TIM_getTimeMS();
            return;
        }
        else if ((start_time + pdMS_TO_TICKS(100)) > HW_TIM_getTimeMS())
        {
            return; /**< wait atleast 100ms to sample the voltages for the first time */
        }

        max_chip.config.sampling           = false;
        max_chip.config.diagnostic_enabled = false;

        MAX_readWriteToChip();
        MAX_readWriteToChip(); /**< Re-read to get updated undervoltage information */

        max_chip.state.connected_cells = BMS_CONFIGURED_SERIES_CELLS;

        if (max_chip.state.connected_cells == 0)
        {
            start_time = 0;
            return;
        }
        else if (max_chip.state.connected_cells != BMS_CONFIGURED_SERIES_CELLS)
        {
            BMS.state = BMS_ERROR;
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
    else if (BMS.fault)
    {
        MAX_readWriteToChip();

        if (!max_chip.state.ready)
        {
            return;
        }
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

    if (BMS.state == BMS_CALIBRATING)
    {
        return;
    }

#if FEATURE_MAX14921_CALIBRATE_1HZ
    max_chip.config.diagnostic_enabled = true;
    max_chip.config.sampling           = true;
    max_chip.config.low_power_mode     = false;
    max_chip.config.balancing          = 0x00;
    max_chip.config.output.state       = MAX_PACK_VOLTAGE;
    max_chip.config.output.output.cell = MAX_CELL1; /**< Prepare for next step */
    MAX_readWriteToChip();

    max_chip.config.sampling_start = HW_TIM_getTimeMS();
    BMS.state                            = BMS_DIAGNOSTIC;
#endif // FEATURE_MAX14921_CALIBRATE_1Hz
}

void BMS_toSleep(void)
{
    BMS.state = BMS_SLEEPING;
    max_chip.config.diagnostic_enabled = false;
    max_chip.config.sampling           = false;
    max_chip.config.low_power_mode     = true;
    max_chip.config.balancing          = 0x00;
    max_chip.config.output.state       = MAX_PACK_VOLTAGE;
    max_chip.config.output.output.cell = MAX_CELL1; /**< Prepare for next step */
    max_chip.config.sampling_start = UINT32_MAX;
    MAX_readWriteToChip();
    MAX_readWriteToChip(); // Update value

    BMS.discharge_limit = 0.0f;
    BMS.charge_limit = 0.0f;
}

void BMS_wakeUp(void)
{
    if (BMS.state != BMS_SLEEPING) return;

    max_chip.config.diagnostic_enabled = false;
    max_chip.config.sampling           = false;
    max_chip.config.low_power_mode     = false;
    max_chip.config.balancing          = 0x00;
    max_chip.config.output.state       = MAX_PACK_VOLTAGE;
    max_chip.config.output.output.cell = MAX_CELL1; /**< Prepare for next step */
    MAX_readWriteToChip();

    while (!max_chip.state.ready) MAX_readWriteToChip(); // Update value
    BMS.state = BMS_WAITING;
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
    for (uint8_t i = 0; i < BMS.connected_cells; i++)
    {
        BMS.cells[i].voltage = IO.cell[i] + BMS.cells[i].parasitic_corr;
        if ((BMS.cells[i].voltage > 2.0f) && (BMS.cells[i].voltage < 4.5f))
        {
#if FEATURE_BMSW_FAULTS
            if (BMS.cells[i].voltage < 2.5f)
            {
                BMS.cells[i].state = BMS_CELL_FAULT_UV;
                continue;
            }
            else if (BMS.cells[i].voltage > 4.2f)
            {
                BMS.cells[i].state = BMS_CELL_FAULT_OV;
                continue;
            }
#endif // FEATURE_BMSW_FAULTS

            BMS.cells[i].state    = BMS_CELL_CONNECTED;
        }
        else
        {
            BMS.cells[i].state = BMS_CELL_ERROR;
        }
    }

    float32_t batt_tmp  = 0;
    uint8_t   tmp_count = 0;

    BMS.voltage.max             = 0x00;
    BMS.voltage.min             = 5.0f;
    BMS.voltage.avg             = 0x00;
    BMS.calculated_pack_voltage = 0x00;
    BMS.relative_soc.max        = 0x00;
    BMS.relative_soc.min        = 101.0f;


    for (uint8_t i = 0; i < max_chip.state.connected_cells; i++)
    {
        if (BMS.cells[i].state == BMS_CELL_ERROR)
        {
            BMS.cells[i].relative_soc = 0;
            continue;
        }

        tmp_count++;
        BMS.voltage.max = (BMS.voltage.max > BMS.cells[i].voltage) ? BMS.voltage.max : BMS.cells[i].voltage;
        BMS.voltage.min = (BMS.voltage.min < BMS.cells[i].voltage) ? BMS.voltage.min : BMS.cells[i].voltage;
        batt_tmp += BMS.cells[i].voltage;
        BMS.calculated_pack_voltage += BMS.cells[i].voltage;

        BMS.cells[i].relative_soc = CELL_getSoCfromV((BMS.cells[i].voltage));
    }

    BMS.voltage.avg = batt_tmp / tmp_count;

    if (BMS.voltage.min <= 5.0f && BMS.voltage.min >= 5.0f)
    {
        BMS.voltage.min = 0;
    }

    BMS.relative_soc.min = CELL_getSoCfromV((BMS.voltage.min));
    BMS.relative_soc.max = CELL_getSoCfromV((BMS.voltage.max));
    BMS.relative_soc.avg = CELL_getSoCfromV((BMS.voltage.avg));

    BMS_checkFault();    // If cells are in error, it will override from sampling state

    BMS_dischargeLimit();
    BMS_chargeLimit();
}

/**
 * @brief  Checks for errors relative to the cells.
 */
void BMS_checkFault(void)
{
    bool faulted = false;

    for (uint8_t i = 0; i < BMS.connected_cells; i++)
    {
        /**< Check if any cell between first and last populated cells in the stack are disconnected*/
        if (BMS.cells[i].state != BMS_CELL_CONNECTED)
        {
            BMS.fault = true;
        }
    }

    if (!max_chip.state.ready || max_chip.state.va_undervoltage || max_chip.state.vp_undervoltage)
    {
        BMS.fault = true;
        faulted = true;
    }

    BMS.fault = faulted;
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
}

/**
 * @brief Callback function for measurement completion
 */
void BMS_measurementComplete(void)
{
    if (BMS.state == BMS_HOLDING)
    {
        BMS.state = BMS_WAITING;
#if FEATURE_CELL_BALANCING
        if (CANRX_get_signal_timeSinceLastMessageMS(VEH, TOOLING_targetBalancingVoltage) < BMS_CONFIGURED_BALANCING_TIMEOUT)
        {
            static bool even = false;

            max_chip.config.balancing = 0x00;
            max_chip.config.low_power_mode = false;

            for (uint8_t i = (even) ? 0 : 1; i < BMS.connected_cells; i += 2)
            {
                max_chip.config.balancing |= (BMS.cells[i].voltage > (CANRX_get_signal(VEH, TOOLING_targetBalancingVoltage) + BMS_CONFIGURED_BALANCING_MARGIN)) ? 1 << i : 0x00;
            }

            even = (even == false);
        }
        else
#endif // FEATURE_CELL_BALANCING
        {
            max_chip.config.low_power_mode = true;
            max_chip.config.balancing = 0x00;
        }
        MAX_readWriteToChip();
    }
    else if (BMS.state == BMS_PARASITIC_MEASUREMENT)
    {
        for (uint8_t i = 0; i < BMS.connected_cells; i++)
        {
            BMS.cells[i].parasitic_corr = (IO.cell[i]) / 128;
            BMS.state                   = BMS_WAITING;
        }
    }
}

void BMS_chargeLimit()
{
    if (ENV.values.max_temp > 60.0f || BMS.fault || BMS.state == BMS_ERROR || BMS.state == BMS_SLEEPING)
    {
        BMS.charge_limit = 0;
        return;
    }

    if (BMS.relative_soc.max <= 80)
    {
        BMS.charge_limit = STANDARD_CHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;
    }
    else
    {
        BMS.charge_limit = ((100.0f - BMS.relative_soc.max)/20.0f) * STANDARD_CHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;    // linear function for the last 20% of charge
    }

    if (ENV.values.max_temp >= 48)
    {
         BMS.charge_limit += -((ENV.values.max_temp - 48.0f)/12.0f) * STANDARD_CHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;
    }

    if (BMS.charge_limit < 0.0f) BMS.charge_limit = 0.0f;
    if (BMS.charge_limit > STANDARD_CHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS) BMS.charge_limit = STANDARD_CHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;
}

void BMS_dischargeLimit()
{
    static uint32_t start_derate = 0x00;

    if (ENV.values.max_temp > 60.0f || BMS.fault || BMS.state == BMS_ERROR || BMS.state == BMS_SLEEPING)
    {
        BMS.discharge_limit = 0.0f;
        return;
    }

    if (BMS.relative_soc.min > 20.0f)
    {
        BMS.discharge_limit = MAX_CONTINOUS_DISCHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;
        start_derate = 0x00;
    }
    else
    {
        if (start_derate == 0x00)
        {
            start_derate = HW_TIM_getTimeMS();
        }
        else if ((start_derate + BMS_CONFIGURED_DERATING_DELAY) < HW_TIM_getTimeMS())
        {
            start_derate = 0x00;
            float32_t dis = BMS.discharge_limit;

            dis -= 1.0f;

            BMS.discharge_limit =  (dis > ((BMS.relative_soc.avg / 20.0f) * MAX_CONTINOUS_DISCHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS)) ? dis : (BMS.relative_soc.avg / 20.0f) * MAX_CONTINOUS_DISCHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;    // linear function for the last 20% of discharge
        }
    }

    if (ENV.values.max_temp >= 48.0f)
    {
        BMS.discharge_limit += -((ENV.values.max_temp - 48.0f) / 12.0f) * MAX_CONTINOUS_DISCHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;
    }

    if (BMS.discharge_limit < 0.0f) BMS.discharge_limit = 0.0f;
    if (BMS.discharge_limit > MAX_CONTINOUS_DISCHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS) BMS.charge_limit = MAX_CONTINOUS_DISCHARGE_CURRENT * BMS_CONFIGURED_PARALLEL_CELLS;
}
