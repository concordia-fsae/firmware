/**
 * @file lib_simpleFilter.h
 * @brief  Header file for simple filters
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
    uint32_t raw;
    float32_t value;
    uint16_t  count;
} lib_simpleFilter_cumAvg_S;

typedef struct
{
    float32_t smoothing_factor;

    float32_t y;
    float32_t y_1;
} lib_simpleFilter_lpf_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void      lib_simpleFilter_cumAvg_clear(lib_simpleFilter_cumAvg_S* filter);
void      lib_simpleFilter_cumAvg_increment(lib_simpleFilter_cumAvg_S* filter, uint32_t sum);
float32_t lib_simpleFilter_cumAvg_average(lib_simpleFilter_cumAvg_S* filter);
