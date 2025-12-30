/**
 * @file lib_simpleFilter.c
 * @brief  Source file for simple filters
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_simpleFilter.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Clear a simple averaging filter
 * @param filter Filter to clear
 */
void lib_simpleFilter_cumAvg_clear(lib_simpleFilter_cumAvg_S* filter)
{
    filter->raw = 0U;
    filter->count = 0U;
}

/**
 * @brief Add value to simple filter
 * @param filter Filter to increment
 * @param sum Value to increment by
 */
void lib_simpleFilter_cumAvg_increment(lib_simpleFilter_cumAvg_S* filter, uint32_t sum)
{
    filter->raw += sum;
    filter->count++;
}

/**
 * @brief Calculate value of simple filter
 * @param filter Filter to average
 */
float32_t lib_simpleFilter_cumAvg_average(lib_simpleFilter_cumAvg_S* filter)
{
    filter->value = (filter->count != 0) ? (((float32_t)filter->raw) / ((float32_t)filter->count)) : 0.0f;
    return filter->value;
}
