/**
 * @file lib_interpolation.c
 * @brief Source file for the linear inteprolation library
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_interpolation.h"

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static float32_t lib_interpolation_private_doInterpolation(lib_interpolation_point_S *const start,
                                                           lib_interpolation_point_S *const stop,
                                                           float32_t x)
{
    const float32_t dx2x1 = stop->x - start->x;
    const float32_t dy2y1 = stop->y - start->y;
    const float32_t dxx1 = x - start->x;
    return start->y + dy2y1 * (dxx1 / dx2x1);
}

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * @brief Initialize an interpolation mapping
 * @param mapping Pointer to mapping
 * @param init_value The value the mapping should start at
 */
void lib_interpolation_init(lib_interpolation_mapping_S * const mapping, float32_t init_value)
{
    mapping->result = init_value;
}

/**
 * @brief Interpolate a value by a mapping
 * @param mapping Pointer to mapping
 * @param x The value to inteprolate
 * @return The value corresponding to x in the mapping
 */
float32_t lib_interpolation_interpolate(lib_interpolation_mapping_S * const mapping, float32_t x)
{
    if (x <= mapping->points[0].x)
    {
        return (mapping->saturate_left) ? mapping->points[0].y :
            lib_interpolation_private_doInterpolation(&mapping->points[0], &mapping->points[1], x);
    }

    for (uint8_t i = 0; i < mapping->number_points - 1; i++)
    {
        if (mapping->points[i + 1].x > x)
        {
            return lib_interpolation_private_doInterpolation(&mapping->points[i], &mapping->points[i + 1], x);
        }
    }

    return (mapping->saturate_right) ? mapping->points[mapping->number_points - 1].y :
        lib_interpolation_private_doInterpolation(&mapping->points[mapping->number_points - 2],
                                                  &mapping->points[mapping->number_points - 1],
                                                  x);
}
