#pragma once

#define VEHICLESTATE_CANRX_SIGNAL          CANRX_get_signal_func(VEH, VCPDU_vehicleState)
#define VEHICLESTATE_CANRX_RESET_SWITCH    CANRX_get_signal_func(VEH, VCPDU_safetyReset)
