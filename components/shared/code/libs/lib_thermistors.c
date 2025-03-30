/**
 * @file lib_thermistors.c
 * @brief  Source file for Thermistor Library
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_thermistors.h"
#include "lib_utility.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Convert Temp from resistance with B parameter
 *
 * @param b_param Characteristics of thermistor
 * @param resistance Resistance of thermistor
 *
 * @retval Temperature of thermistor in Kelvin
 */
float32_t lib_thermistors_getKelvinFromR_BParameter(const lib_thermistors_BParameter_S* b_param, float32_t resistance)
{
    float32_t ret = 0.0F;

    ret = (1.0F / b_param->T0) + ((1.0F / b_param->B) * ln(resistance / b_param->R0));

    return 1.0F / ret;
}
