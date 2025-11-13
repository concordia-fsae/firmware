/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/
#include "drv_inputAD_config.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

drv_inputAD_configAnalog_S drv_inputAD_configAnalog[${len(channels)}] = {
%for name, channel in channels.items():
    [DRV_INPUTAD_ANALOG_${name}] = {
        .multiplier = DRV_INPUTAD_ANALOG_MULTIPLIER_${name},
    },
%endfor
};
