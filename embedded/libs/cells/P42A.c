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
float32_t CELL_getSoCfromV(float32_t volt)
{
    
    float32_t ret = 0.0f;

    //if (volt <= 2.7f || volt >= 4.17f)
    //{
    //    return 0;
    //}

    //if (volt <= 3.407f) {
    //    ret = (28.177f*volt*volt-151.31f*volt+202.91f);
    //}
    //else if (volt <= 3.44f) {
    //    ret = (132.01f*volt-435.2f);
    //}
    //else if (volt <= 4.054f) {
    //    ret = (109.75f*volt-359.17f);
    //}
    //else if (volt <= 4.094f) {
    //    ret = (-7496.4f*volt*volt+61360.0f*volt-125467.0f);
    //}
    //else if (volt > 4.094f) {
    //    ret = (-498.39f*volt*volt+4165.6f*volt-8604.0f);
    //}

    //if (ret < 0.0f) ret = 0.0f;
    //if (ret > 100.0f) ret = 100.0f;
    
    if (volt > 4.2f)
    {
        ret = 100.0;
    }
    else if (volt < 2.2f)
    {
        ret = 0.0f;
    }
    else if (volt > 3.2f)
    {
        ret = (volt - 3.2f) * 80.0f + 20.0f;
    }
    else if (volt > 2.4f)
    {
        ret = ((volt - 2.4f) / 0.6f) * 10.0f + 10.0f;
    }
    else if (volt > 2.0f)
    {
        ret = ((volt - 2.0f) / 0.4f) * 10.0f;
    }

    return ret;
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
