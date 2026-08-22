#pragma once

#include "drv_inputAD.h"
#include "HW_gpio.h"
#include "LIB_Types.h"

#include <stdbool.h>

void      rig_runtime_set_analog_input(drv_inputAD_channelAnalog_E channel, float32_t voltage);
float32_t rig_runtime_get_analog_input(drv_inputAD_channelAnalog_E channel);
void      rig_runtime_set_digital_io(HW_GPIO_pinmux_E channel, bool state);
bool      rig_runtime_get_digital_io(HW_GPIO_pinmux_E channel);
