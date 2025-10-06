#include "drv_inputAD_config.h"

drv_inputAD_configAnalog_S drv_inputAD_configAnalog[${len(channels)}] = {
%for name, channel in channels.items():
    [DRV_INPUTAD_ANALOG_${name}] = {
        .multiplier = DRV_INPUTAD_ANALOG_MULTIPLIER_${name},
    },
%endfor
};
