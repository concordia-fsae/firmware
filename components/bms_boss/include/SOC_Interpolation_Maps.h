/**
 * @file SOC_Interpolation_Maps.h
 * @brief Interpolation maps for cell parameters
 */

#include "lib_interpolation.h"
#include "lib_utility.h"

#define Resistance_offset 0.0f  //Andrew?


/******************************************************************************
 *                             Interpolations Maps
 ******************************************************************************/

static lib_interpolation_point_S SOC_OCVMap[] = {
    {.x = 0.0f,  .y = 2.42f},   //SOC to OCV
    {.x = 10.0f, .y = 3.178f},
    {.x = 20.0f, .y = 3.37f},
    {.x = 30.0f, .y = 3.52f},
    {.x = 40.0f, .y = 3.62f},
    {.x = 50.0f, .y = 3.75f},
    {.x = 60.0f, .y = 3.84f},
    {.x = 70.0f, .y = 3.94f},
    {.x = 80.0f, .y = 4.05f},
    {.x = 90.0f, .y = 4.09f},
    {.x = 100.0f, .y = 4.20f},
};

static lib_interpolation_mapping_S SOC_OCV_Func = {
    .points = (lib_interpolation_point_S*)&SOC_OCVMap,
    .number_points = COUNTOF(SOC_OCVMap),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_point_S SOC_dOCVMap[] = {
    {.x = 0.0f, .y = 7.58f}, //SOC to dOCV
    {.x = 10.0f, .y = 4.75f},
    {.x = 20.0f, .y = 1.71f},
    {.x = 30.0f, .y = 1.25f},
    {.x = 40.0f, .y = 1.15f},
    {.x = 50.0f, .y = 1.1f},
    {.x = 60.0f, .y = 0.95f},
    {.x = 70.0f, .y = 1.05f},
    {.x = 80.0f, .y = 0.75f},
    {.x = 90.0f, .y = 0.75f},
    {.x = 100.0f, .y = 1.1f},
};

static lib_interpolation_mapping_S SOC_dOCV_Func = {
    .points = (lib_interpolation_point_S*)&SOC_dOCVMap,
    .number_points = COUNTOF(SOC_dOCVMap),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_point_S SOC_RiMap[] = {
    {.x = 0.0f, .y = 0.0074 + Resistance_offset}, //SOC to Ri
    {.x = 10.0f, .y = 0.0074 + Resistance_offset},
    {.x = 20.0f, .y = 0.0068 + Resistance_offset},
    {.x = 30.0f, .y = 0.0063 + Resistance_offset},
    {.x = 40.0f, .y = 0.0062 + Resistance_offset},
    {.x = 50.0f, .y = 0.0062 + Resistance_offset},
    {.x = 60.0f, .y = 0.0054+ Resistance_offset},
    {.x = 70.0f, .y = 0.0056 + Resistance_offset},
    {.x = 80.0f, .y = 0.0065 + Resistance_offset},
    {.x = 90.0f, .y = 0.0066 + Resistance_offset},
    {.x = 100.0f, .y = 0.0079 + Resistance_offset},
};

static lib_interpolation_mapping_S SOC_Ri_FUNC = {
    .points = (lib_interpolation_point_S*)&SOC_RiMap,
    .number_points = COUNTOF(SOC_RiMap),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_point_S SOC_R1Map[] = {
    {.x = 0.0f, .y = 0.0049}, //SOC to R1
    {.x = 10.0f, .y = 0.0049f},
    {.x = 20.0f, .y = 0.0032f},
    {.x = 30.0f, .y = 0.002f},
    {.x = 40.0f, .y = 0.0019f},
    {.x = 50.0f, .y = 0.0019f},
    {.x = 60.0f, .y = 0.0021f},
    {.x = 70.0f, .y = 0.0024f},
    {.x = 80.0f, .y = 0.0028f},
    {.x = 90.0f, .y = 0.0023f},
    {.x = 100.0f, .y = 0.0045f},
};

static lib_interpolation_mapping_S SOC_R1_FUNC = {
    .points = (lib_interpolation_point_S*)&SOC_R1Map,
    .number_points = COUNTOF(SOC_R1Map),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_point_S SOC_C1Map[] = {
    {.x = 0.0f, .y = 697.3229f}, //SOC to C1
    {.x = 10.0f, .y = 697.3229f},
    {.x = 20.0f, .y = 1748.00f},
    {.x = 30.0f, .y = 1935.5f},
    {.x = 40.0f, .y = 1587.1f},
    {.x = 50.0f, .y = 1456.5f},
    {.x = 60.0f, .y = 447.43f},
    {.x = 70.0f, .y = 671.78f},
    {.x = 80.0f, .y = 1376.00f},
    {.x = 90.0f, .y = 1266.2f},
    {.x = 100.0f, .y = 625.55f},
};

static lib_interpolation_mapping_S SOC_C1_FUNC = {
    .points = (lib_interpolation_point_S*)&SOC_C1Map,
    .number_points = COUNTOF(SOC_C1Map),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_point_S SOC_R2Map[] = {
    {.x = 0.0f, .y = 0.1063f}, //SOC to R2
    {.x = 10.0f, .y = 0.1063f},
    {.x = 20.0f, .y = 0.0781f},
    {.x = 30.0f, .y = 0.0167f},
    {.x = 40.0f, .y = 0.012f},
    {.x = 50.0f, .y = 0.0098f},
    {.x = 60.0f, .y = 0.0074f},
    {.x = 70.0f, .y = 0.0142f},
    {.x = 80.0f, .y = 0.0126f},
    {.x = 90.0f, .y = 0.0091f},
    {.x = 100.0f, .y = 0.0274f},
};

static lib_interpolation_mapping_S SOC_R2_FUNC = {
    .points = (lib_interpolation_point_S*)&SOC_R2Map,
    .number_points = COUNTOF(SOC_R2Map),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_point_S SOC_C2Map[] = {
    {.x = 0.0f, .y = 1.25E+03f}, //SOC to C2
    {.x = 10.0f, .y = 1.25E+03f},
    {.x = 20.0f, .y = 2234.9f},
    {.x = 30.0f, .y = 2972.5f},
    {.x = 40.0f, .y = 3368.7f},
    {.x = 50.0f, .y = 3599.5f},
    {.x = 60.0f, .y = 3273.0f},
    {.x = 70.0f, .y = 2650.9f},
    {.x = 80.0f, .y = 3200.6f},
    {.x = 90.0f, .y = 3107.3f},
    {.x = 100.0f, .y = 2047.4f},
};

static lib_interpolation_mapping_S SOC_C2_FUNC = {
    .points = (lib_interpolation_point_S*)&SOC_C2Map,
    .number_points = COUNTOF(SOC_C2Map),
    .saturate_left = true,
    .saturate_right = true,
};

