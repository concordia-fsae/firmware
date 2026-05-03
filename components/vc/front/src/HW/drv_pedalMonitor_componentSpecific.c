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
lib_interpolation_point_S mapping_apps1[21U] = {
    { .x = 0.525f, .y = 0.00f, },
    { .x = 0.586f, .y = 0.05f, },
    { .x = 0.648f, .y = 0.10f, },
    { .x = 0.726f, .y = 0.15f, },
    { .x = 0.787f, .y = 0.20f, },
    { .x = 0.843f, .y = 0.25f, },
    { .x = 0.915f, .y = 0.30f, },
    { .x = 0.972f, .y = 0.35f, },
    { .x = 1.028f, .y = 0.40f, },
    { .x = 1.094f, .y = 0.45f, },
    { .x = 1.159f, .y = 0.50f, },
    { .x = 1.208f, .y = 0.55f, },
    { .x = 1.268f, .y = 0.60f, },
    { .x = 1.322f, .y = 0.65f, },
    { .x = 1.381f, .y = 0.70f, },
    { .x = 1.428f, .y = 0.75f, },
    { .x = 1.498f, .y = 0.80f, },
    { .x = 1.553f, .y = 0.85f, },
    { .x = 1.590f, .y = 0.90f, },
    { .x = 1.612f, .y = 0.95f, },
    { .x = 1.622f, .y = 1.00f, },
};

/**
 * @member x [V] Pedal pot voltage
 * @member y [%] Pedal position 0.0f-1.0f | 0.0f = 0%, 1.0f = 100%
 */
lib_interpolation_point_S mapping_apps2[21U] = {
    { .x = 1.415f, .y = 0.00f, },
    { .x = 1.441f, .y = 0.05f, },
    { .x = 1.469f, .y = 0.10f, },
    { .x = 1.489f, .y = 0.15f, },
    { .x = 1.518f, .y = 0.20f, },
    { .x = 1.549f, .y = 0.25f, },
    { .x = 1.572f, .y = 0.30f, },
    { .x = 1.603f, .y = 0.35f, },
    { .x = 1.634f, .y = 0.40f, },
    { .x = 1.660f, .y = 0.45f, },
    { .x = 1.687f, .y = 0.50f, },
    { .x = 1.722f, .y = 0.55f, },
    { .x = 1.751f, .y = 0.60f, },
    { .x = 1.783f, .y = 0.65f, },
    { .x = 1.812f, .y = 0.70f, },
    { .x = 1.848f, .y = 0.75f, },
    { .x = 1.873f, .y = 0.80f, },
    { .x = 1.904f, .y = 0.85f, },
    { .x = 1.936f, .y = 0.90f, },
    { .x = 1.949f, .y = 0.95f, },
    { .x = 1.954f, .y = 1.00f, },
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
    { .x = 0.3f,   .y = 0.0f,  },
    { .x = 2.7f,   .y = 1.0f,  },
};

drv_pedalMonitor_channelConfig_S drv_pedalMonitor_channels[DRV_PEDALMONITOR_CHANNEL_COUNT] = {
    [DRV_PEDALMONITOR_APPS1] = {
        .type = DRV_PEDALMONITOR_TYPE_ANALOG,
        .input.analog = {
            .channel = DRV_INPUTAD_ANALOG_APPS_P1,
            .fault_high = 2.5f,
            .fault_low = 0.40f,
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
            .fault_high = 2.85f,
            .fault_low = 0.15f,
            .pedal_map = {
                .points = (lib_interpolation_point_S*)&mapping_brake_pr,
                .number_points = COUNTOF(mapping_brake_pr),
                .saturate_left = true,
                .saturate_right = true,
            },
        }
    },
};
