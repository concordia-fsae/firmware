/**
 * BMSW simulation shim for the component-specific drv_inputAD layer.
 *
 * The native BMSW model and Python segment publish demuxed logical analog
 * channels through Rig. This shim exposes those values through the public
 * drv_inputAD API without instantiating the private inputAD storage layer.
 */

#include "BatteryMonitoring.h"
#include "drv_inputAD.h"
#include "HW_gpio.h"
#include "HW_MAX14921.h"
#include "io.h"
#include "ModuleDesc.h"

float32_t drv_inputAD_getAnalogVoltage(drv_inputAD_channelAnalog_E channel)
{
    return rig_runtime_get_analog_input(channel);
}

drv_io_logicLevel_E drv_inputAD_getLogicLevel(drv_inputAD_channelDigital_E channel)
{
    switch (channel)
    {
#if APP_VARIANT_ID == 1U
        case DRV_INPUTAD_DIGITAL_NSHUTDOWN:
            return rig_runtime_get_digital_io(HW_GPIO_NSHUTDOWN)
                       ? DRV_IO_LOGIC_HIGH
                       : DRV_IO_LOGIC_LOW;
#endif
        default:
            return DRV_IO_LOGIC_LOW;
    }
}

drv_io_activeState_E drv_inputAD_getDigitalActiveState(drv_inputAD_channelDigital_E channel)
{
    return (drv_inputAD_getLogicLevel(channel) == DRV_IO_LOGIC_LOW)
               ? DRV_IO_ACTIVE
               : DRV_IO_INACTIVE;
}

static void drv_inputAD_init_componentSpecific(void) {}

static void drv_inputAD_1kHz_PRD(void)
{
    // Preserve the MAX14921 measurement sequencing from the production
    // component driver. Analog values themselves are already demuxed and
    // provided by Rig, so this shim deliberately does not use private inputAD
    // storage or ADC-bank unpacking.
    if ((BMS.state == BMS_HOLDING) || (BMS.state == BMS_PARASITIC_MEASUREMENT))
    {
        const MAX_selectedCell_E current_cell = BMS_getCurrentOutputCell();
        if (current_cell == MAX_CELL1)
        {
            BMS_measurementComplete();
        }
        else if (BMS.delayed_measurement)
        {
            BMS.delayed_measurement = false;
        }
        else
        {
            BMS_setOutputCell(current_cell - 1);
        }
    }
}

const ModuleDesc_S drv_inputAD_desc = {
    .moduleInit       = &drv_inputAD_init_componentSpecific,
    .periodic1kHz_CLK = &drv_inputAD_1kHz_PRD,
};
