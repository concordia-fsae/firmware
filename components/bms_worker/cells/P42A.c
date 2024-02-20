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
    // sets tenth_mv to volts to be used in calculations
    uint16_t volt = tenth_mv/10000;

    if (volt<=3.407) {
        return (uint32_t)(28.177*volt*volt-151.31*volt-202.91);
    }
    else if (volt<=3.44) {
        return (uint32_t)(-132.01*volt-435.2);
    }
    else if(volt<=4.054) {
        return (uint32_t)(109.75*volt-359.17);
    }
    else if(volt<=4.094) {
        return (uint32_t)(-7496.4*volt*volt+61360*volt-125467);
    }
    else if(volt>4.094) {
        return (uint32_t)(-498.39*volt*volt+4165.6*volt-8604);
    }
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
