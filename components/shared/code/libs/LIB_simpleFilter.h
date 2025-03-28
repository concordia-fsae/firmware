/**
 * @file LIB_simpleFilter.h
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
} LIB_simpleFilter_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void LIB_simpleFilter_clear(LIB_simpleFilter_S* filter);
void LIB_simpleFilter_increment(LIB_simpleFilter_S* filter, uint32_t sum);
float32_t LIB_simpleFilter_average(LIB_simpleFilter_S* filter);
