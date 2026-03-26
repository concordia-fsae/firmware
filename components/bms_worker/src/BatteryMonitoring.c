/**
 * @file BatteryMonitoring.c
 * @brief  Source code for Battery Monitoring Application
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module header */
#include "BatteryMonitoring.h"
#include "Yamcan.h"

/**< Driver Includes */
#include "HW.h"
#include "HW_adc.h"
#include "HW_tim.h"
#include "drv_inputAD.h"
#include "drv_timer.h"

/**< Other Includes */
#include "Module.h"
#include "string.h"

#include "app_faultManager.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define BMS_CONFIGURED_BALANCING_TIMEOUT 1100
#define BMS_CONFIGURED_BALANCING_MARGIN 0.050f // [V], precision 1mV
#define BMS_START_DELAY_MS 100U

#define BMS_CONFIGURED_SAMPLING_TIME_MS 20
#define BMS_CONFIGURED_BALANCING_TIME_MS 500

#define CELL_DISCONNECTED_TIMEOUT_MS 15000U
#define CELL_UNDERVOLTAGE_TIMEOUT_MS 1000U
#define CELL_CHIPERROR_TIMEOUT_MS 500U

#define CELL_DISCONNECTED_THRESH_LOW  1.5f
#define CELL_DISCONNECTED_THRESH_HIGH 4.5f

#define CELL_VOLTAGE_THRESH_UV 2.0f
#define CELL_VOLTAGE_THRESH_OV 4.225f

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern MAX_S max_chip;

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

BMS_S BMS;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct bms_S
{
    drv_timer_S timerDisconnected;
    drv_timer_S timerUndervoltage;
    drv_timer_S timerChipError;
} bms;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Checks for errors relative to the cells.
 */
static void checkFault(void)
{
    bool hasUv = false, hasOv = false, hasNc = false;
    const bool analogError = max_chip.state.va_undervoltage;
    const bool packError = max_chip.state.vp_undervoltage;
    const bool chipError = analogError || packError;

    for (uint8_t i = 0; i < BMS_CONFIGURED_SERIES_CELLS; i++)
    {
        hasUv |= BMS.cells[i].state == BMS_CELL_FAULT_UV;
        hasOv |= BMS.cells[i].state == BMS_CELL_FAULT_OV;
        hasNc |= BMS.cells[i].state == BMS_CELL_ERROR;
    }

    const bool cellDisconnected = drv_timer_run(&bms.timerDisconnected, hasNc) == DRV_TIMER_EXPIRED;
    const bool cellUndervoltage = drv_timer_run(&bms.timerUndervoltage, hasUv) == DRV_TIMER_EXPIRED;
    const bool chipInError = drv_timer_run(&bms.timerChipError, chipError) == DRV_TIMER_EXPIRED;

    app_faultManager_setFaultState(FM_FAULT_BMSW_CELLDISCONNECTED, cellDisconnected);
    app_faultManager_setFaultState(FM_FAULT_BMSW_CELLUNDERVOLTAGE, cellUndervoltage);
    app_faultManager_setFaultState(FM_FAULT_BMSW_CELLOVERVOLTAGE, hasOv);
    app_faultManager_setFaultState(FM_FAULT_BMSW_ANALOGREF5VHWERROR, analogError);
    app_faultManager_setFaultState(FM_FAULT_BMSW_PACKVOLTAGEHWERROR, packError);

    BMS.fault = cellDisconnected || cellUndervoltage || chipInError || hasOv;
}

/**
 * @brief  Calculates the segments statistics.
 */
static void calcSegStats(void)
{
    for (uint8_t i = 0; i < BMS_CONFIGURED_SERIES_CELLS; i++)
    {
        BMS.cells[i].voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_CELL1 + i) + BMS.cells[i].parasitic_corr;
        if ((BMS.cells[i].voltage > CELL_DISCONNECTED_THRESH_LOW) &&
            (BMS.cells[i].voltage < CELL_DISCONNECTED_THRESH_HIGH))
        {
            if (BMS.cells[i].voltage < CELL_VOLTAGE_THRESH_UV)
            {
                BMS.cells[i].state = BMS_CELL_FAULT_UV;
                continue;
            }
            else if (BMS.cells[i].voltage > CELL_VOLTAGE_THRESH_OV)
            {
                BMS.cells[i].state = BMS_CELL_FAULT_OV;
                continue;
            }
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

    for (uint8_t i = 0; i < BMS_CONFIGURED_SERIES_CELLS; i++)
    {
        tmp_count++;
        BMS.voltage.max = (BMS.voltage.max > BMS.cells[i].voltage) ? BMS.voltage.max : BMS.cells[i].voltage;
        BMS.voltage.min = (BMS.voltage.min < BMS.cells[i].voltage) ? BMS.voltage.min : BMS.cells[i].voltage;
        batt_tmp += BMS.cells[i].voltage;
        BMS.calculated_pack_voltage += BMS.cells[i].voltage;
    }

    BMS.voltage.avg = batt_tmp / tmp_count;

    checkFault();    // If cells are in error, it will override from sampling state
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

MAX_selectedCell_E BMS_getCurrentOutputCell(void)
{
    return max_chip.config.output.output.cell;
}

/**
 * @brief  Set output cell of the cell multiplexer
 *
 * @param cell Voltage of cell to output
 */
void BMS_setOutputCell(MAX_selectedCell_E cell)
{
    MAX_setOutputCell(cell);
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

            for (uint8_t i = (even) ? 0 : 1; i < BMS_CONFIGURED_SERIES_CELLS; i += 2)
            {
                max_chip.config.balancing |= (BMS.cells[i].voltage > (CANRX_get_signal(VEH, TOOLING_targetBalancingVoltage) + BMS_CONFIGURED_BALANCING_MARGIN)) ? 1 << i : 0x00;
            }

            even = (even == false);
        }
        else
#endif // FEATURE_CELL_BALANCING
        {
            max_chip.config.balancing = 0x00;
        }
        MAX_readWriteToChip();
    }
    else if (BMS.state == BMS_PARASITIC_MEASUREMENT)
    {
        for (uint8_t i = 0; i < BMS_CONFIGURED_SERIES_CELLS; i++)
        {
            BMS.cells[i].parasitic_corr = (drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_CELL1 + i)) / 128.0f;
            BMS.state                   = BMS_WAITING;
        }
    }

    max_chip.config.diagnostic_enabled = false;
    max_chip.config.sampling           = false;
    max_chip.config.low_power_mode     = false;
    max_chip.config.output.state       = MAX_AMPLIFIER_SELF_CALIBRATION;
    max_chip.config.output.output.cell = BMS_CONFIGURED_SERIES_CELLS - 1; /**< Prepare for next step */
    MAX_readWriteToChip();
    BMS.state = BMS_CALIBRATING;

    calcSegStats();
}

/**
 * @brief  BMS Module init function
 */
static void BMS_Init(void)
{
    memset(&BMS, 0, sizeof(BMS));

    drv_timer_initWithRuntime(&bms.timerDisconnected, CELL_DISCONNECTED_TIMEOUT_MS);
    drv_timer_initWithRuntime(&bms.timerUndervoltage, CELL_UNDERVOLTAGE_TIMEOUT_MS);
    drv_timer_initWithRuntime(&bms.timerChipError, CELL_CHIPERROR_TIMEOUT_MS);

    if (!MAX_init())
    {
        BMS.state = BMS_ERROR;
    }
    else
    {
        max_chip.config.diagnostic_enabled = false;
        max_chip.config.sampling           = false;
        max_chip.config.diagnostic_enabled = false;
        max_chip.config.low_power_mode     = false;
        max_chip.config.balancing          = 0x00;
        max_chip.config.output.state       = MAX_PARASITIC_ERROR_CALIBRATION;

        MAX_readWriteToChip();
        max_chip.config.sampling_start = HW_TIM_getTimeMS();
        BMS.state                      = BMS_PARASITIC;
    }
}

/**
 * @brief  10 Hz BMS periodic function
 */
static void BMS100Hz_PRD()
{
    if ((BMS.state == BMS_PARASITIC) &&
        (HW_TIM_getTimeMS() >= (max_chip.config.sampling_start + BMS_CONFIGURED_SAMPLING_TIME_MS)))
    {
        max_chip.config.sampling_start     = UINT32_MAX;
        max_chip.config.sampling           = false;
        max_chip.config.diagnostic_enabled = false;
        max_chip.config.low_power_mode     = false;
        max_chip.config.balancing          = 0x00;
        max_chip.config.output.state       = MAX_CELL_VOLTAGE;
        max_chip.config.output.output.cell = BMS_CONFIGURED_SERIES_CELLS - 1; /**< Prepare for next step */
        BMS.delayed_measurement            = true;
        BMS.state                          = BMS_PARASITIC_MEASUREMENT;
    }
    else if (BMS.state == BMS_SAMPLING)
    {
        if (max_chip.config.sampling_start == UINT32_MAX)
        {
            max_chip.config.sampling           = true;
            max_chip.config.diagnostic_enabled = false;
            max_chip.config.low_power_mode     = false;
            max_chip.config.balancing          = 0x00;
            max_chip.config.output.state       = MAX_PACK_VOLTAGE;
            max_chip.config.output.output.cell = BMS_CONFIGURED_SERIES_CELLS - 1; /**< Prepare for next step */

            MAX_readWriteToChip();
            max_chip.config.sampling_start = HW_TIM_getTimeMS();
            return;
        }
        else if (HW_TIM_getTimeMS() >= (max_chip.config.sampling_start + BMS_CONFIGURED_SAMPLING_TIME_MS))
        {
            max_chip.config.sampling_start       = UINT32_MAX;
            BMS.pack_voltage                     = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_SEGMENT) * 16;
            BMS_setOutputCell(BMS_CONFIGURED_SERIES_CELLS - 1);
            BMS.delayed_measurement              = true;
            BMS.state                            = BMS_HOLDING;
        }
    }
    else if (BMS.state == BMS_WAITING)
    {
        BMS.state                            = BMS_SAMPLING;
        max_chip.config.sampling_start       = UINT32_MAX;
    }
    else if (BMS.state == BMS_CALIBRATING)
    {
        MAX_readWriteToChip();

        if (max_chip.state.ready)
        {
            max_chip.config.diagnostic_enabled = true;
            max_chip.config.sampling           = true;
            max_chip.config.low_power_mode     = false;
            max_chip.config.output.state       = MAX_PACK_VOLTAGE;
            max_chip.config.output.output.cell = BMS_CONFIGURED_SERIES_CELLS - 1; /**< Prepare for next step */
            MAX_readWriteToChip();

            max_chip.config.sampling_start = HW_TIM_getTimeMS();
            BMS.state                      = BMS_DIAGNOSTIC;
        }
    }
    else if (BMS.state == BMS_DIAGNOSTIC)
    {
        if ((max_chip.config.sampling_start + BMS_CONFIGURED_SAMPLING_TIME_MS) < HW_TIM_getTimeMS())
        {
            max_chip.config.sampling             = false;
            max_chip.config.diagnostic_enabled   = false;
            max_chip.config.low_power_mode       = false;
            max_chip.config.balancing            = 0x00;
            max_chip.config.output.state         = MAX_PACK_VOLTAGE;
            max_chip.config.output.output.cell   = BMS_CONFIGURED_SERIES_CELLS - 1; /**< Prepare for next step */
            MAX_readWriteToChip();
            BMS.state                            = BMS_SAMPLING;
            max_chip.config.sampling_start       = UINT32_MAX;
        }
    }
}

/**
 * @brief  BMS Module descriptor
 */
const ModuleDesc_S BMS_desc = {
    .moduleInit       = &BMS_Init,
    .periodic100Hz_CLK = &BMS100Hz_PRD,
};
