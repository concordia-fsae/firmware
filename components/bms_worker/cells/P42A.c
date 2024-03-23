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

/**
 * @brief  Calculates State of Charge from voltage
 *
 * @param volt 0.1 mV
 *
 * @retval unit: % from 0-100  
 */
uint8_t CELL_getSoCfromV(uint16_t tenth_mv)
{
    // sets tenth_mv to volts to be used in calculations
    float32_t volt = (float)tenth_mv/10000;

    // add *2 for more precision later (to use 200 of the 255 available numbers with the uint8_t)

    if (volt<=3.407) {
        return (28.177f*volt*volt-151.31f*volt+202.91f);
    }
    else if (volt<=3.44) {
        return (132.01f*volt-435.2f);
    }
    else if (volt<=4.054) {
        return (109.75f*volt-359.17f);
    }
    else if (volt<=4.094) {
        return (-7496.4f*volt*volt+61360.0f*volt-125467.0f);
    }
    else if (volt>4.094) {
        return (-498.39f*volt*volt+4165.6f*volt-8604.0f);
    }
    return 0;
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
