#ifndef DRV_INPUTAD_CONFIG_H
#define DRV_INPUTAD_CONFIG_H

#pragma once

typedef struct {
    float multiplier;
} drv_inputAD_configAnalog_S;

/*Adding enum for the array indices - NOT 100% sure about this*/
typedef enum {
%for i, name in enumerate(channels.keys()):
    DRV_INPUTAD_ANALOG_${name} = ${i},
%endfor
} drv_inputAD_configAnalog_E;

%for name, channel in channels.items():
#define DRV_INPUTAD_ANALOG_MULTIPLIER_${name}    ${channel["multiplier"]}f
%endfor

extern drv_inputAD_configAnalog_S drv_inputAD_configAnalog[${len(channels)}];

#endif /* DRV_INPUTAD_CONFIG_H */