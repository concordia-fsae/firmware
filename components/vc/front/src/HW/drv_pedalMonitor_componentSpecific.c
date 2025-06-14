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
    { .x = 0.947f, .y = 1.0f, },
    { .x = 1.015f, .y = .95f, },
    { .x = 1.064f, .y = 0.9f, },
    { .x = 1.108f, .y = .85f, },
    { .x = 1.156f, .y = 0.8f, },
    { .x = 1.190f, .y = .75f, },
    { .x = 1.243f, .y = 0.7f, },
    { .x = 1.263f, .y = .65f, },
    { .x = 1.278f, .y = 0.6f, },
    { .x = 1.309f, .y = .55f, },
    { .x = 1.353f, .y = 0.5f, },
    { .x = 1.403f, .y = .45f, },
    { .x = 1.423f, .y = 0.4f, },
    { .x = 1.44f, .y = .35f, },
    { .x = 1.488f, .y = 0.3f, },
    { .x = 1.536f, .y = .25f, },
    { .x = 1.583f, .y = 0.2f, },
    { .x = 1.614f, .y = .15f, },
    { .x = 1.635f, .y = 0.1f, },
    { .x = 1.658f, .y = .05f, },
    { .x = 1.677f, .y = 0.0f, },
};

/**
 * @member x [V] Pedal pot voltage
 * @member y [%] Pedal position 0.0f-1.0f | 0.0f = 0%, 1.0f = 100%
 */
lib_interpolation_point_S mapping_apps2[] = {
    { .x = 1.952f, .y = 1.0f, },
    { .x = 1.939f, .y = .95f, },
    { .x = 1.924f, .y = 0.9f, },
    { .x = 1.910f, .y = .85f, },
    { .x = 1.892f, .y = 0.8f, },
    { .x = 1.862f, .y = .75f, },
    { .x = 1.833f, .y = 0.7f, },
    { .x = 1.802f, .y = .65f, },
    { .x = 1.794f, .y = 0.6f, },
    { .x = 1.779f, .y = .55f, },
    { .x = 1.751f, .y = 0.5f, },
    { .x = 1.725f, .y = .45f, },
    { .x = 1.708f, .y = 0.4f, },
    { .x = 1.698f, .y = .35f, },
    { .x = 1.687f, .y = 0.3f, },
    { .x = 1.657f, .y = 0.25f, },
    { .x = 1.633f, .y = 0.2f, },
    { .x = 1.612f, .y = .15f, },
    { .x = 1.589f, .y = 0.1f, },
    { .x = 1.553f, .y = 0.05f},
    { .x = 1.517f, .y = 0.0f, },
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

lib_interpolation_point_S mapping_brake_pr[] = {
    { .x = 0.5f,   .y = 0.0f,  },
    { .x = 2.5f,   .y = 1.0f,  },
};

drv_pedalMonitor_channelConfig_S drv_pedalMonitor_channels[DRV_PEDALMONITOR_CHANNEL_COUNT] = {
    [DRV_PEDALMONITOR_APPS1] = {
        .type = DRV_PEDALMONITOR_TYPE_ANALOG,
        .input.analog = {
            .channel = DRV_INPUTAD_ANALOG_APPS_P1,
            .fault_high = 2.5f,
            .fault_low = 0.5f,
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
            .fault_high = 2.5f,
            .fault_low = 0.5f,
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
    [DRV_PEDALMONITOR_BRAKE_PR] = {
        .type = DRV_PEDALMONITOR_TYPE_ANALOG,
        .input.analog = {
            .channel = DRV_INPUTAD_ANALOG_BR_PR,
            .fault_high = 4.75f,
            .fault_low = 0.25f,
            .pedal_map = {
                .points = (lib_interpolation_point_S*)&mapping_brake_pr,
                .number_points = COUNTOF(mapping_brake_pr),
                .saturate_left = true,
                .saturate_right = true,
            },
        }
    },
};
