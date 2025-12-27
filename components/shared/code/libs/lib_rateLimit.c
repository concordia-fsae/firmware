/**
 * lib_rateLimit.c
 * Source code for the rate limiting library
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_rateLimit.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t lib_rateLimit_linear_update(lib_rateLimit_linear_S* linear, float32_t x_n)
{
    const bool increment = x_n > linear->y_n;

    if ((x_n > (linear->y_n + linear->maxStepDelta)) ||
        (x_n < (linear->y_n - linear->maxStepDelta)))
    {
        linear->y_n += increment ? linear->maxStepDelta : -linear->maxStepDelta;
    }
    else
    {
        linear->y_n = x_n;
    }

    return linear->y_n;
}
