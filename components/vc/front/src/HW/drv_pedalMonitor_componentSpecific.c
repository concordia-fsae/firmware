/**
 * @file drv_pedalMonitor_componentSpecific.c
 * @brief Header file for pedal monitor
 * @note Pedal positon is a float percentage between 0.0f and 1.0f where 
 *       0.0f is 0% and 1.0f is 100%
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_pedalMonitor.h"
#include "Utility.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

/**
 * @member x [V] Pedal pot voltage
 * @member y [%] Pedal position 0.0f-1.0f | 0.0f = 0%, 1.0f = 100%
 */
lib_interpolation_point_S mapping_apps1[] = {
    { .x = 0.528f, .y = 1.0f,  },
    { .x = 0.689f, .y = 0.8f,  },
    { .x = 0.857f, .y = 0.65f, },
    { .x = 1.010f, .y = 0.5f,  },
    { .x = 1.172f, .y = 0.35f, },
    { .x = 1.333f, .y = 0.2f,  },
    { .x = 1.494f, .y = 0.1f,  },
    { .x = 1.654f, .y = 0.05f, },
    { .x = 1.815f, .y = 0.0f,  },
};

/**
 * @member x [V] Pedal pot voltage
 * @member y [%] Pedal position 0.0f-1.0f | 0.0f = 0%, 1.0f = 100%
 */
lib_interpolation_point_S mapping_apps2[] = {
    { .x = 0.937f, .y = 1.0f,  },
    { .x = 1.059f, .y = 0.8f,  },
    { .x = 1.181f, .y = 0.65f, },
    { .x = 1.316f, .y = 0.5f,  },
    { .x = 1.436f, .y = 0.35f, },
    { .x = 1.562f, .y = 0.2f,  },
    { .x = 1.685f, .y = 0.1f,  },
    { .x = 1.808f, .y = 0.05f, },
    { .x = 1.931f, .y = 0.0f,  },
};

// TODO: Calibrate
/**
 * @member x [V] Pedal pot voltage
 * @member y [%] Pedal position 0.0f-1.0f | 0.0f = 0%, 1.0f = 100%
 */
lib_interpolation_point_S mapping_brake_pot[] = {
    { .x = 0.5f,   .y = 0.0f,  },
    { .x = 1.5f,   .y = 1.0f,  },
};

drv_pedalMonitor_channelConfig_S drv_pedalMonitor_channels[DRV_PEDALMONITOR_CHANNEL_COUNT] = {
    [DRV_PEDALMONITOR_APPS1] = {
        .type = DRV_PEDALMONITOR_TYPE_ANALOG,
        .input.analog = {
            .channel = DRV_INPUTAD_ANALOG_APPS_P1,
            .fault_high = 1.9f,
            .fault_low = 0.4f,
            .pedal_map = {
                .points = (lib_interpolation_point_S*)&mapping_apps1,
                .number_points = COUNTOF(mapping_apps1),
                .saturate_left = true,
                .saturate_right = true,
            },
        }
    },
    [DRV_PEDALMONITOR_APPS2] = {
        .type = DRV_PEDALMONITOR_TYPE_ANALOG,
        .input.analog = {
            .channel = DRV_INPUTAD_ANALOG_APPS_P2,
            .fault_high = 2.05f,
            .fault_low = 0.8f,
            .pedal_map = {
                .points = (lib_interpolation_point_S*)&mapping_apps2,
                .number_points = COUNTOF(mapping_apps2),
                .saturate_left = true,
                .saturate_right = true,
            },
        }
    },
    [DRV_PEDALMONITOR_BRAKE_POT] = {
        .type = DRV_PEDALMONITOR_TYPE_ANALOG,
        .input.analog = {
            .channel = DRV_INPUTAD_ANALOG_BR_POT,
            .fault_high = 0.0f,
            .fault_low = 0.0f,
            .pedal_map = {
                .points = (lib_interpolation_point_S*)&mapping_brake_pot,
                .number_points = COUNTOF(mapping_brake_pot),
                .saturate_left = true,
                .saturate_right = true,
            },
        }
    },
};
