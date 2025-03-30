/**
 * @file lib_interpolation.h
 * @brief Header file for the linear inteprolation library
 *
 * Setup
 * 1. Declare an array (at whatever size required >= 2) of lib_interpolation_point_S
 *    points. The array shall be ordered such that each index i will have an x
 *    parameter less than i+1. In other words:
 *    - point[i].x < point[i + 1].x | -inf < i < inf
 * 2. Use the COUNTOF(points) macro to specify the number of points in the
 *    points mapping
 * 3. If you want the value to saturate on the left boundary (when viewing a graph)
 *    set the saturate left member to true. The same is applicable to the right.
 *    In other words:
 *    - f(c) = (c < point[0].x) ? point[0].x : interpolate(c)
 * 4. Initialize a point with lib_interpolation_init
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t x;
    float32_t y;
} lib_interpolation_point_S;

typedef struct
{
    lib_interpolation_point_S * points;
    const uint8_t               number_points;
    const bool                  saturate_left;
    const bool                  saturate_right;
    float32_t                   result;
} lib_interpolation_mapping_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void      lib_interpolation_init(lib_interpolation_mapping_S * const mapping, float32_t init_value);
float32_t lib_interpolation_interpolate(lib_interpolation_mapping_S * const mapping, float32_t x);
