#include "drv_inputAD_componentSpecific.h"

drv_inputAD_configAnalog_S drv_inputAD_configAnalog[%{len(channels)}] = {
%for channel in channels:
    [DRV_INPUTAD_${channel.name}] = {
        .multiplier = DRV_INPUTAD_ANALOG_MULTIPLIER_${channel.name},
    },
%endfor
};
