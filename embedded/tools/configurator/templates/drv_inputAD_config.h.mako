/*
 * drv_inputAD_config.h
 */

#pragma once

%for name, channel in channels.items():
#define DRV_INPUTAD_ANALOG_MULTIPLIER_${name}    ${channel["multiplier"]}f
%endfor
