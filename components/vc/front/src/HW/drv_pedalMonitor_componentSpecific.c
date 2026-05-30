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
    { .x = 0.820f, .y = 0.05f, },
    { .x = 0.904f, .y = 0.10f, },
    { .x = 0.963f, .y = 0.15f, },
    { .x = 1.026f, .y = 0.20f, },
    { .x = 1.079f, .y = 0.25f, },
    { .x = 1.127f, .y = 0.30f, },
    { .x = 1.180f, .y = 0.35f, },
    { .x = 1.231f, .y = 0.40f, },
    { .x = 1.270f, .y = 0.45f, },
    { .x = 1.313f, .y = 0.50f, },
    { .x = 1.354f, .y = 0.55f, },
    { .x = 1.394f, .y = 0.60f, },
    { .x = 1.432f, .y = 0.65f, },
    { .x = 1.468f, .y = 0.70f, },
    { .x = 1.515f, .y = 0.75f, },
    { .x = 1.552f, .y = 0.80f, },
    { .x = 1.578f, .y = 0.85f, },
    { .x = 1.596f, .y = 0.90f, },
    { .x = 1.608f, .y = 0.95f, },
    { .x = 1.615f, .y = 1.00f, },
};

/**
 * @member x [V] Pedal pot voltage
 * @member y [%] Pedal position 0.0f-1.0f | 0.0f = 0%, 1.0f = 100%
 */
lib_interpolation_point_S mapping_apps2[21U] = {
    { .x = 1.475f, .y = 0.00f, },
    { .x = 1.523f, .y = 0.05f, },
    { .x = 1.549f, .y = 0.10f, },
    { .x = 1.582f, .y = 0.15f, },
    { .x = 1.608f, .y = 0.20f, },
    { .x = 1.636f, .y = 0.25f, },
    { .x = 1.664f, .y = 0.30f, },
    { .x = 1.687f, .y = 0.35f, },
    { .x = 1.710f, .y = 0.40f, },
    { .x = 1.738f, .y = 0.45f, },
    { .x = 1.761f, .y = 0.50f, },
    { .x = 1.785f, .y = 0.55f, },
    { .x = 1.809f, .y = 0.60f, },
    { .x = 1.833f, .y = 0.65f, },
    { .x = 1.857f, .y = 0.70f, },
    { .x = 1.876f, .y = 0.75f, },
    { .x = 1.898f, .y = 0.80f, },
    { .x = 1.920f, .y = 0.85f, },
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
