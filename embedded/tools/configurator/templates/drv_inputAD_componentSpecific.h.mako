#pragma once

%for channel in channels:
#define DRV_INPUTAD_ANALOG_MULTIPLIER_${channel.name}    1.0f
%endfor
