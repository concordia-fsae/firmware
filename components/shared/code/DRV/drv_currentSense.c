/**
 * @file drv_currentSense.c
 * @brief  Source file for generic current sensing function
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_currentSense.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t drv_currentSense_voltageToCurrent(
    float32_t   voltage,
    float32_t   amp_per_volt
)
{
    return amp_per_volt * voltage;
}