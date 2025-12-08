/*
 * drv_inputAD_config.h
 */

#pragma once

#include "drv_inputAD.h"

extern drv_inputAD_configAnalog_S drv_inputAD_configAnalog[${len(channels)}];

%for name, channel in channels.items():
#define DRV_INPUTAD_ANALOG_MULTIPLIER_${name}    ${channel["multiplier"]}f
%endfor
