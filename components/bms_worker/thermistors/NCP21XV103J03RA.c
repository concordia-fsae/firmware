/**
 * @file NCP21XV103J03RA.c
 * @brief  Source code for NCP21XV103J03RA Thermistor
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Other Includes
#include "NCP21XV103J03RA.h"
#include "Utility.h"


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
float NCP21_GetTempFromR_BParameter(THERM_BParameter_S* params, float r)
{
    float ret = 0.0F;

    ret = 1 / params->T0 + (1 / params->B) * ln(r / params->R0);

    return 1 / ret;
}
