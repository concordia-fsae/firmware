#include "io.h"

#include "runtime_state.h"

#include "HW.h"

static float32_t rig_runtime_analog_inputs[DRV_INPUTAD_ANALOG_COUNT];

void drv_vn9008_run(void) __attribute__((weak));
void drv_inputAD_private_setAnalogVoltage(
    drv_inputAD_channelAnalog_E channel,
    float32_t                   voltage) __attribute__((weak));

void rig_runtime_set_analog_input(drv_inputAD_channelAnalog_E channel, float32_t voltage)
{
    if (channel < DRV_INPUTAD_ANALOG_COUNT)
    {
        rig_runtime_analog_inputs[channel] = voltage;
    }

    if (channel < ADC_BANK1_CHANNEL_COUNT)
    {
        rig_runtime.bank1[channel] = voltage;
    }
    else
    {
        const uint8_t bank2_channel = (uint8_t)channel - ADC_BANK1_CHANNEL_COUNT;
        if (bank2_channel < ADC_BANK2_CHANNEL_COUNT)
        {
            rig_runtime.bank2[bank2_channel] = voltage;
        }
    }

    if (drv_inputAD_private_setAnalogVoltage != NULL)
    {
        drv_inputAD_private_setAnalogVoltage(channel, voltage);
    }
    if (drv_vn9008_run != NULL)
    {
        drv_vn9008_run();
    }
}

float32_t rig_runtime_get_analog_input(drv_inputAD_channelAnalog_E channel)
{
    return (channel < DRV_INPUTAD_ANALOG_COUNT) ? rig_runtime_analog_inputs[channel] : 0.0f;
}

void rig_runtime_set_digital_io(HW_GPIO_pinmux_E channel, bool state)
{
    if (channel < HW_GPIO_COUNT)
    {
        rig_runtime.gpio[channel] = state;
    }
}

bool rig_runtime_get_digital_io(HW_GPIO_pinmux_E channel)
{
    return (channel < HW_GPIO_COUNT) ? rig_runtime.gpio[channel] : false;
}

HW_StatusTypeDef_E HW_ADC_init(void)
{
    return HW_OK;
}

HW_StatusTypeDef_E HW_ADC_deInit(void)
{
    return HW_OK;
}

void HW_ADC_unpackADCBuffer(void)
{
}

float32_t HW_ADC_getVFromBank1Channel(HW_adcChannels_bank1_E channel)
{
    return (channel < ADC_BANK1_CHANNEL_COUNT) ? rig_runtime.bank1[channel] : 0.0f;
}

float32_t HW_ADC_getVFromBank2Channel(HW_adcChannels_bank2_E channel)
{
    return (channel < ADC_BANK2_CHANNEL_COUNT) ? rig_runtime.bank2[channel] : 0.0f;
}

HW_StatusTypeDef_E HW_GPIO_init(void)
{
    return HW_OK;
}

HW_StatusTypeDef_E HW_GPIO_deInit(void)
{
    return HW_OK;
}

bool HW_GPIO_readPin(HW_GPIO_pinmux_E pin)
{
    return rig_runtime_get_digital_io(pin);
}

void HW_GPIO_writePin(HW_GPIO_pinmux_E pin, bool state)
{
    rig_runtime_set_digital_io(pin, state);
}

void HW_GPIO_togglePin(HW_GPIO_pinmux_E pin)
{
    rig_runtime_set_digital_io(pin, !rig_runtime_get_digital_io(pin));
}
