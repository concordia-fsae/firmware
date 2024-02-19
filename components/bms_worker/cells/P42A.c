/**
 * @file P42A.c
 * @brief  Source code for Molicel P42A Cell Library functions
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CELL.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Calculates the cell capacity in 0.0001Ah from a Voltage in 0.0001V
 *
 * @param tenth_mv unit: 0.1mV
 *
 * @retval unit: 0.1mAh
 */
uint32_t CELL_getCapacityfromV(uint16_t tenth_mv)
{
    return (uint32_t)(-2.3061*tenth_mv*tenth_mv*10000*10000+12.868*tenth_mv*10000-13.827);
}

/**
 * @brief  Calculates cell voltage from capacity
 *
 * @param tenth_mah 0.1 mAh
 *
 * @retval unit: 0.1mv
 */
uint16_t CELL_getVfromCapacity(uint32_t tenth_mah)
{
    return tenth_mah / 4.2 * 1.7 + 2.5;
}
