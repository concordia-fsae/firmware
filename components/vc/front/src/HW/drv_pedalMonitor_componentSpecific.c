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
    { .x = 0.773f, .y = 0.00f, },
    { .x = 0.817f, .y = 0.05f, },
    { .x = 0.862f, .y = 0.10f, },
    { .x = 0.919f, .y = 0.15f, },
    { .x = 0.963f, .y = 0.20f, },
    { .x = 1.004f, .y = 0.25f, },
    { .x = 1.056f, .y = 0.30f, },
    { .x = 1.097f, .y = 0.35f, },
    { .x = 1.138f, .y = 0.40f, },
    { .x = 1.186f, .y = 0.45f, },
    { .x = 1.233f, .y = 0.50f, },
    { .x = 1.268f, .y = 0.55f, },
    { .x = 1.312f, .y = 0.60f, },
    { .x = 1.351f, .y = 0.65f, },
    { .x = 1.394f, .y = 0.70f, },
    { .x = 1.428f, .y = 0.75f, },
    { .x = 1.479f, .y = 0.80f, },
    { .x = 1.519f, .y = 0.85f, },
    { .x = 1.545f, .y = 0.90f, },
    { .x = 1.561f, .y = 0.95f, },
    { .x = 1.569f, .y = 1.00f, },
};

/**
 * @member x [V] Pedal pot voltage
 * @member y [%] Pedal position 0.0f-1.0f | 0.0f = 0%, 1.0f = 100%
 */
lib_interpolation_point_S mapping_apps2[21U] = {
    { .x = 1.525f, .y = 0.00f, },
    { .x = 1.545f, .y = 0.05f, },
    { .x = 1.566f, .y = 0.10f, },
    { .x = 1.580f, .y = 0.15f, },
    { .x = 1.602f, .y = 0.20f, },
    { .x = 1.625f, .y = 0.25f, },
    { .x = 1.643f, .y = 0.30f, },
    { .x = 1.666f, .y = 0.35f, },
    { .x = 1.689f, .y = 0.40f, },
    { .x = 1.708f, .y = 0.45f, },
    { .x = 1.729f, .y = 0.50f, },
    { .x = 1.755f, .y = 0.55f, },
    { .x = 1.776f, .y = 0.60f, },
    { .x = 1.800f, .y = 0.65f, },
    { .x = 1.822f, .y = 0.70f, },
    { .x = 1.849f, .y = 0.75f, },
    { .x = 1.868f, .y = 0.80f, },
    { .x = 1.891f, .y = 0.85f, },
    { .x = 1.915f, .y = 0.90f, },
    { .x = 1.924f, .y = 0.95f, },
    { .x = 1.928f, .y = 1.00f, },
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
