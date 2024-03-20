/**
 * @file THERMISTORS.c
 * @brief  Source code for Thermistor Library
 */
 
/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Utility.h"
#include "THERMISTORS.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Convert Temp from resistance with B parameter
 *
 * @param params Characteristics of thermistor
 * @param r Resistance of thermistor
 *
 * @retval Temperature of thermistor
 */
float THERM_GetTempFromR_BParameter(THERM_BParameter_S* params, float r)
{
    float ret = 0.0F;

    ret = 1 / params->T0 + (1 / params->B) * ln(r / params->R0);

    return 1 / ret;
}

