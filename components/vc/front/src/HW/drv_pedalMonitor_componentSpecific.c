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
    { .x = 0.720f, .y = 0.00f, },
    { .x = 0.843f, .y = 0.05f, },
    { .x = 0.927f, .y = 0.10f, },
    { .x = 0.989f, .y = 0.15f, },
    { .x = 1.054f, .y = 0.20f, },
    { .x = 1.104f, .y = 0.25f, },
    { .x = 1.155f, .y = 0.30f, },
    { .x = 1.208f, .y = 0.35f, },
    { .x = 1.253f, .y = 0.40f, },
    { .x = 1.292f, .y = 0.45f, },
    { .x = 1.334f, .y = 0.50f, },
    { .x = 1.372f, .y = 0.55f, },
    { .x = 1.413f, .y = 0.60f, },
    { .x = 1.445f, .y = 0.65f, },
    { .x = 1.485f, .y = 0.70f, },
    { .x = 1.527f, .y = 0.75f, },
    { .x = 1.560f, .y = 0.80f, },
    { .x = 1.582f, .y = 0.85f, },
    { .x = 1.598f, .y = 0.90f, },
    { .x = 1.609f, .y = 0.95f, },
    { .x = 1.615f, .y = 1.00f, },
};

/**
 * @member x [V] Pedal pot voltage
 * @member y [%] Pedal position 0.0f-1.0f | 0.0f = 0%, 1.0f = 100%
 */
lib_interpolation_point_S mapping_apps2[21U] = {
    { .x = 1.475f, .y = 0.00f, },
    { .x = 1.532f, .y = 0.05f, },
    { .x = 1.572f, .y = 0.10f, },
    { .x = 1.605f, .y = 0.15f, },
    { .x = 1.634f, .y = 0.20f, },
    { .x = 1.663f, .y = 0.25f, },
    { .x = 1.689f, .y = 0.30f, },
    { .x = 1.713f, .y = 0.35f, },
    { .x = 1.736f, .y = 0.40f, },
    { .x = 1.759f, .y = 0.45f, },
    { .x = 1.780f, .y = 0.50f, },
    { .x = 1.803f, .y = 0.55f, },
    { .x = 1.824f, .y = 0.60f, },
    { .x = 1.846f, .y = 0.65f, },
    { .x = 1.867f, .y = 0.70f, },
    { .x = 1.887f, .y = 0.75f, },
    { .x = 1.906f, .y = 0.80f, },
    { .x = 1.922f, .y = 0.85f, },
    { .x = 1.934f, .y = 0.90f, },
    { .x = 1.941f, .y = 0.95f, },
    { .x = 1.945f, .y = 1.00f, },
};

// TODO: Calibrate
/**
 * @member x [V] Pedal pot voltage
 * @member y [%] Pedal position 0.0f-1.0f | 0.0f = 0%, 1.0f = 100%
 */
lib_interpolation_point_S        mapping_brake_pr[] = {
    { .x = 0.3f, .y = 0.0f, },
    { .x = 2.7f, .y = 1.0f, },
};

drv_pedalMonitor_channelConfig_S drv_pedalMonitor_channels[DRV_PEDALMONITOR_CHANNEL_COUNT] = {
    [DRV_PEDALMONITOR_APPS1] =    {
        .type         = DRV_PEDALMONITOR_TYPE_ANALOG,
        .input.analog =           {
            .channel    = DRV_INPUTAD_ANALOG_APPS_P1,
            .fault_high =                                       2.5f,
            .fault_low  =                                      0.40f,
            .pedal_map  =         {
                .points         = (lib_interpolation_point_S*)&mapping_apps1,
                .number_points  = COUNTOF(mapping_apps1),
                .saturate_left  = true,
                .saturate_right = true,
            },
        }
    },
    [DRV_PEDALMONITOR_APPS2] =    {
        .type         = DRV_PEDALMONITOR_TYPE_ANALOG,
        .input.analog =           {
            .channel    = DRV_INPUTAD_ANALOG_APPS_P2,
            .fault_high =                                       2.5f,
            .fault_low  =                                       0.5f,
            .pedal_map  =         {
                .points         = (lib_interpolation_point_S*)&mapping_apps2,
                .number_points  = COUNTOF(mapping_apps2),
                .saturate_left  = true,
                .saturate_right = true,
            },
        }
    },
    [DRV_PEDALMONITOR_BRAKE_PR] = {
        .type         = DRV_PEDALMONITOR_TYPE_ANALOG,
        .input.analog =           {
            .channel    = DRV_INPUTAD_ANALOG_BR_PR,
            .fault_high =                                         2.85f,
            .fault_low  =                                         0.15f,
            .pedal_map  =         {
                .points         = (lib_interpolation_point_S*)&mapping_brake_pr,
                .number_points  = COUNTOF(mapping_brake_pr),
                .saturate_left  = true,
                .saturate_right = true,
            },
        }
    },
};
