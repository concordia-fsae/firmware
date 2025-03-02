/**
 * @file lib_thermistors.h
 * @brief  Header file for Thermistor Library
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_FloatTypes.h"
#include "lib_utility.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define KELVIN_OFFSET 273.15F
#define lib_thermistors_getCelsiusFromR_BParameter(bparam, resistance) (lib_thermistors_getKelvinFromR_BParameter(bparam, resistance) - KELVIN_OFFSET)

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t B;
    float32_t T0;
    float32_t R0;
} lib_thermistors_BParameter_S;

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

static const lib_thermistors_BParameter_S MF52_bParam = { // For the MF52C1103F3380
    .B  = 3380U,
    .T0 = 25U + KELVIN_OFFSET,
    .R0 = 10000U,
};

static const lib_thermistors_BParameter_S NCP21_bParam = { // For the NCP21XV103J03RA
    .B  = 3930U,
    .T0 = 25U + KELVIN_OFFSET,
    .R0 = 10000U,
};

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
static float32_t lib_thermistors_getKelvinFromR_BParameter(const lib_thermistors_BParameter_S* b_param, float32_t resistance)
{
    float32_t ret = 0.0F;

    ret = (1.0F / b_param->T0) + ((1.0F / b_param->B) * ln(resistance / b_param->R0));

    return 1.0F / ret;
}
