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
    { .x = 0.802f, .y = 0.05f, },
    { .x = 0.882f, .y = 0.10f, },
    { .x = 0.945f, .y = 0.15f, },
    { .x = 1.007f, .y = 0.20f, },
    { .x = 1.059f, .y = 0.25f, },
    { .x = 1.112f, .y = 0.30f, },
    { .x = 1.163f, .y = 0.35f, },
    { .x = 1.209f, .y = 0.40f, },
    { .x = 1.261f, .y = 0.45f, },
    { .x = 1.309f, .y = 0.50f, },
    { .x = 1.351f, .y = 0.55f, },
    { .x = 1.390f, .y = 0.60f, },
    { .x = 1.429f, .y = 0.65f, },
    { .x = 1.469f, .y = 0.70f, },
    { .x = 1.509f, .y = 0.75f, },
    { .x = 1.543f, .y = 0.80f, },
    { .x = 1.576f, .y = 0.85f, },
    { .x = 1.600f, .y = 0.90f, },
    { .x = 1.620f, .y = 0.95f, },
    { .x = 1.628f, .y = 1.00f, },
};

/**
 * @member x [V] Pedal pot voltage
 * @member y [%] Pedal position 0.0f-1.0f | 0.0f = 0%, 1.0f = 100%
 */
lib_interpolation_point_S mapping_apps2[21U] = {
    { .x = 1.475f, .y = 0.00f, },
    { .x = 1.523f, .y = 0.05f, },
    { .x = 1.555f, .y = 0.10f, },
    { .x = 1.584f, .y = 0.15f, },
    { .x = 1.613f, .y = 0.20f, },
    { .x = 1.640f, .y = 0.25f, },
    { .x = 1.666f, .y = 0.30f, },
    { .x = 1.690f, .y = 0.35f, },
    { .x = 1.716f, .y = 0.40f, },
    { .x = 1.738f, .y = 0.45f, },
    { .x = 1.762f, .y = 0.50f, },
    { .x = 1.786f, .y = 0.55f, },
    { .x = 1.809f, .y = 0.60f, },
    { .x = 1.831f, .y = 0.65f, },
    { .x = 1.853f, .y = 0.70f, },
    { .x = 1.875f, .y = 0.75f, },
    { .x = 1.895f, .y = 0.80f, },
    { .x = 1.914f, .y = 0.85f, },
    { .x = 1.928f, .y = 0.90f, },
    { .x = 1.942f, .y = 0.95f, },
    { .x = 1.950f, .y = 1.00f, },
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
