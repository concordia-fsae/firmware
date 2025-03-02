/**
 * @file LIB_simpleFilter.c
 * @brief  Source file for simple filters
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_simpleFilter.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Clear a simple averaging filter
 * @param filter Filter to clear
 */
void LIB_simpleFilter_clear(LIB_simpleFilter_S* filter)
{
    filter->raw = 0U;
    filter->count = 0U;
}

/**
 * @brief Add value to simple filter
 * @param filter Filter to increment
 * @param value Value to increment by
 */
void LIB_simpleFilter_increment(LIB_simpleFilter_S* filter, float32_t value)
{
    filter->raw += value;
    filter->count++;
}

/**
 * @brief Calculate value of simple filter
 * @param filter Filter to average
 */
float32_t LIB_simpleFilter_average(LIB_simpleFilter_S* filter)
{
    filter->value = (float32_t)filter->raw / (float32_t)filter->count;
    return filter->value;
}
