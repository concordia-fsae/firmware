/*
* drv_inputAD_config.h
 */

#pragma once

%for channel in channels:
#define DRV_INPUTAD_ANALOG_MULTIPLIER_${channel.name}    ${channel.multiplier}f
%endfor
