/**
 * @file SOC_Interpolation_Maps.h
 * @brief Interpolation maps for cell parameters
 */

#include "lib_interpolation.h"
#include "lib_utility.h"

#define Resistance_offset    0.0f // Andrew?


/******************************************************************************
 *                             Interpolations Maps
 ******************************************************************************/

static lib_interpolation_point_S   SOC_OCVMap[] = {
    { .x =   0.0f, .y =  2.42f }, // SOC to OCV
    { .x =  10.0f, .y = 3.178f },
    { .x =  20.0f, .y =  3.37f },
    { .x =  30.0f, .y =  3.52f },
    { .x =  40.0f, .y =  3.62f },
    { .x =  50.0f, .y =  3.75f },
    { .x =  60.0f, .y =  3.84f },
    { .x =  70.0f, .y =  3.94f },
    { .x =  80.0f, .y =  4.05f },
    { .x =  90.0f, .y =  4.09f },
    { .x = 100.0f, .y =  4.20f },
};

static lib_interpolation_mapping_S SOC_OCV_FUNC = {
    .points         = (lib_interpolation_point_S*)&SOC_OCVMap,
    .number_points  = COUNTOF(SOC_OCVMap),
    .saturate_left  = true,
    .saturate_right = true,
};

static lib_interpolation_point_S   SOC_dOCVMap[] = {
    { .x =   0.0f, .y = 7.58f }, // SOC to dOCV
    { .x =  10.0f, .y = 4.75f },
    { .x =  20.0f, .y = 1.71f },
    { .x =  30.0f, .y = 1.25f },
    { .x =  40.0f, .y = 1.15f },
    { .x =  50.0f, .y =  1.1f },
    { .x =  60.0f, .y = 0.95f },
    { .x =  70.0f, .y = 1.05f },
    { .x =  80.0f, .y = 0.75f },
    { .x =  90.0f, .y = 0.75f },
    { .x = 100.0f, .y =  1.1f },
};

static lib_interpolation_mapping_S SOC_dOCV_FUNC = {
    .points         = (lib_interpolation_point_S*)&SOC_dOCVMap,
    .number_points  = COUNTOF(SOC_dOCVMap),
    .saturate_left  = true,
    .saturate_right = true,
};

static lib_interpolation_point_S   SOC_Ri_Discharge_Map[] = {
    { .x =   0.0f, .y = 0.0074f + Resistance_offset }, // SOC to Ri
    { .x =  10.0f, .y = 0.0074f + Resistance_offset },
    { .x =  20.0f, .y = 0.0068f + Resistance_offset },
    { .x =  30.0f, .y = 0.0063f + Resistance_offset },
    { .x =  40.0f, .y = 0.0062f + Resistance_offset },
    { .x =  50.0f, .y = 0.0062f + Resistance_offset },
    { .x =  60.0f, .y = 0.0054f + Resistance_offset },
    { .x =  70.0f, .y = 0.0056f + Resistance_offset },
    { .x =  80.0f, .y = 0.0065f + Resistance_offset },
    { .x =  90.0f, .y = 0.0066f + Resistance_offset },
    { .x = 100.0f, .y = 0.0079f + Resistance_offset },
};

static lib_interpolation_mapping_S SOC_Ri_DISCHARGE_FUNC = {
    .points         = (lib_interpolation_point_S*)&SOC_Ri_Discharge_Map,
    .number_points  = COUNTOF(SOC_Ri_Discharge_Map),
    .saturate_left  = true,
    .saturate_right = true,
};

static lib_interpolation_point_S   SOC_R1_Discharge_Map[] = {
    { .x =   0.0f, .y = 0.0049f }, // SOC to R1
    { .x =  10.0f, .y = 0.0049f },
    { .x =  20.0f, .y = 0.0032f },
    { .x =  30.0f, .y =  0.002f },
    { .x =  40.0f, .y = 0.0019f },
    { .x =  50.0f, .y = 0.0019f },
    { .x =  60.0f, .y = 0.0021f },
    { .x =  70.0f, .y = 0.0024f },
    { .x =  80.0f, .y = 0.0028f },
    { .x =  90.0f, .y = 0.0023f },
    { .x = 100.0f, .y = 0.0045f },
};

static lib_interpolation_mapping_S SOC_R1_DISCHARGE_FUNC = {
    .points         = (lib_interpolation_point_S*)&SOC_R1_Discharge_Map,
    .number_points  = COUNTOF(SOC_R1_Discharge_Map),
    .saturate_left  = true,
    .saturate_right = true,
};

static lib_interpolation_point_S   SOC_C1_Discharge_Map[] = {
    { .x =   0.0f, .y = 697.3229f }, // SOC to C1
    { .x =  10.0f, .y = 697.3229f },
    { .x =  20.0f, .y =  1748.00f },
    { .x =  30.0f, .y =   1935.5f },
    { .x =  40.0f, .y =   1587.1f },
    { .x =  50.0f, .y =   1456.5f },
    { .x =  60.0f, .y =   447.43f },
    { .x =  70.0f, .y =   671.78f },
    { .x =  80.0f, .y =  1376.00f },
    { .x =  90.0f, .y =   1266.2f },
    { .x = 100.0f, .y =   625.55f },
};

static lib_interpolation_mapping_S SOC_C1_DISCHARGE_FUNC = {
    .points         = (lib_interpolation_point_S*)&SOC_C1_Discharge_Map,
    .number_points  = COUNTOF(SOC_C1_Discharge_Map),
    .saturate_left  = true,
    .saturate_right = true,
};

static lib_interpolation_point_S   SOC_R2_Discharge_Map[] = {
    { .x =   0.0f, .y = 0.1063f }, // SOC to R2
    { .x =  10.0f, .y = 0.1063f },
    { .x =  20.0f, .y = 0.0781f },
    { .x =  30.0f, .y = 0.0167f },
    { .x =  40.0f, .y =  0.012f },
    { .x =  50.0f, .y = 0.0098f },
    { .x =  60.0f, .y = 0.0074f },
    { .x =  70.0f, .y = 0.0142f },
    { .x =  80.0f, .y = 0.0126f },
    { .x =  90.0f, .y = 0.0091f },
    { .x = 100.0f, .y = 0.0274f },
};

static lib_interpolation_mapping_S SOC_R2_DISCHARGE_FUNC = {
    .points         = (lib_interpolation_point_S*)&SOC_R2_Discharge_Map,
    .number_points  = COUNTOF(SOC_R2_Discharge_Map),
    .saturate_left  = true,
    .saturate_right = true,
};

static lib_interpolation_point_S   SOC_C2_Discharge_Map[] = {
    { .x =   0.0f, .y = 1.25E+03f }, // SOC to C2
    { .x =  10.0f, .y = 1.25E+03f },
    { .x =  20.0f, .y =   2234.9f },
    { .x =  30.0f, .y =   2972.5f },
    { .x =  40.0f, .y =   3368.7f },
    { .x =  50.0f, .y =   3599.5f },
    { .x =  60.0f, .y =   3273.0f },
    { .x =  70.0f, .y =   2650.9f },
    { .x =  80.0f, .y =   3200.6f },
    { .x =  90.0f, .y =   3107.3f },
    { .x = 100.0f, .y =   2047.4f },
};

static lib_interpolation_mapping_S SOC_C2_DISCHARGE_FUNC = {
    .points         = (lib_interpolation_point_S*)&SOC_C2_Discharge_Map,
    .number_points  = COUNTOF(SOC_C2_Discharge_Map),
    .saturate_left  = true,
    .saturate_right = true,
};

static lib_interpolation_point_S   SOC_Ri_Charge_Map[] = {
    { .x =   0.0f, .y = 0.0081f + Resistance_offset }, // SOC to Ri
    { .x =  10.0f, .y = 0.0081f + Resistance_offset },
    { .x =  20.0f, .y = 0.0071f + Resistance_offset },
    { .x =  30.0f, .y = 0.0032f + Resistance_offset },
    { .x =  40.0f, .y = 0.0056f + Resistance_offset },
    { .x =  50.0f, .y = 0.0058f + Resistance_offset },
    { .x =  60.0f, .y = 0.0061f + Resistance_offset },
    { .x =  70.0f, .y = 0.0057f + Resistance_offset },
    { .x =  80.0f, .y = 0.0061f + Resistance_offset },
    { .x =  90.0f, .y = 0.0062f + Resistance_offset },
    { .x = 100.0f, .y = 0.0084f + Resistance_offset },
};

static lib_interpolation_mapping_S SOC_Ri_CHARGE_FUNC = {
    .points         = (lib_interpolation_point_S*)&SOC_Ri_Charge_Map,
    .number_points  = COUNTOF(SOC_Ri_Charge_Map),
    .saturate_left  = true,
    .saturate_right = true,
};

static lib_interpolation_point_S   SOC_R1_Charge_Map[] = {
    { .x =   0.0f, .y =   0.0110f },
    { .x =  10.0f, .y =   0.0110f },
    { .x =  20.0f, .y =   0.0102f },
    { .x =  30.0f, .y =   0.0033f },
    { .x =  40.0f, .y =   0.0013f },
    { .x =  50.0f, .y =   0.0013f },
    { .x =  60.0f, .y =   0.0014f },
    { .x =  70.0f, .y =   0.0015f },
    { .x =  80.0f, .y =   0.0018f },
    { .x =  90.0f, .y =   0.0019f },
    { .x = 100.0f, .y = 7.94E-06f },
};

static lib_interpolation_mapping_S SOC_R1_CHARGE_FUNC = {
    .points         = (lib_interpolation_point_S*)&SOC_R1_Charge_Map,
    .number_points  = COUNTOF(SOC_R1_Charge_Map),
    .saturate_left  = true,
    .saturate_right = true,
};

static lib_interpolation_point_S   SOC_R2_Charge_Map[] = {
    { .x =   0.0f, .y = 0.0040f },
    { .x =  10.0f, .y = 0.0040f },
    { .x =  20.0f, .y = 0.0030f },
    { .x =  30.0f, .y = 0.0050f },
    { .x =  40.0f, .y = 0.0047f },
    { .x =  50.0f, .y = 0.0046f },
    { .x =  60.0f, .y = 0.0049f },
    { .x =  70.0f, .y = 0.0066f },
    { .x =  80.0f, .y = 0.0066f },
    { .x =  90.0f, .y = 0.0058f },
    { .x = 100.0f, .y = 0.0090f },
};

static lib_interpolation_mapping_S SOC_R2_CHARGE_FUNC = {
    .points         = (lib_interpolation_point_S*)&SOC_R2_Charge_Map,
    .number_points  = COUNTOF(SOC_R2_Charge_Map),
    .saturate_left  = true,
    .saturate_right = true,
};

static lib_interpolation_point_S   SOC_C1_Charge_Map[] = {
    { .x =   0.0f, .y = 898.8253f },
    { .x =  10.0f, .y = 898.8253f },
    { .x =  20.0f, .y =   1460.0f },
    { .x =  30.0f, .y =  13.8447f },
    { .x =  40.0f, .y = 411.8151f },
    { .x =  50.0f, .y = 707.6551f },
    { .x =  60.0f, .y =   1310.0f },
    { .x =  70.0f, .y = 516.2969f },
    { .x =  80.0f, .y = 732.6093f },
    { .x =  90.0f, .y = 653.2170f },
    { .x = 100.0f, .y = 629.4704f },
};

static lib_interpolation_mapping_S SOC_C1_CHARGE_FUNC = {
    .points         = (lib_interpolation_point_S*)&SOC_C1_Charge_Map,
    .number_points  = COUNTOF(SOC_C1_Charge_Map),
    .saturate_left  = true,
    .saturate_right = true,
};

static lib_interpolation_point_S   SOC_C2_Charge_Map[] = {
    { .x =   0.0f, .y =   6060.0f },
    { .x =  10.0f, .y =   6060.0f },
    { .x =  20.0f, .y =  13300.0f },
    { .x =  30.0f, .y =   1930.0f },
    { .x =  40.0f, .y =   2290.0f },
    { .x =  50.0f, .y =   2540.0f },
    { .x =  60.0f, .y =   3420.0f },
    { .x =  70.0f, .y =   2090.0f },
    { .x =  80.0f, .y =   2365.0f },
    { .x =  90.0f, .y =   2574.0f },
    { .x = 100.0f, .y = 700.7608f },
};

static lib_interpolation_mapping_S SOC_C2_CHARGE_FUNC = {
    .points         = (lib_interpolation_point_S*)&SOC_C2_Charge_Map,
    .number_points  = COUNTOF(SOC_C2_Charge_Map),
    .saturate_left  = true,
    .saturate_right = true,
};