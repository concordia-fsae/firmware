/**
 * @file HW_MAX7356.c
 * @brief  Source code for MAX7356EUG+ driver
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-21
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_MAX7356.h"
#include "HW_i2c.h"


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static HW_I2C_Device_S devices[I2C_Bus_Count] = {
    [I2C_Bus1] = {
        .handle = &i2c1,
        .addr   = 0x70 << 1,
    },
    [I2C_Bus2] = {
        .handle = &i2c2,
        .addr   = 0x70 << 1,
    }
};


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Opens or closes the gates on the MAX7356EUG+, works on all configured device busses 
 *
 * @param command Index of gates to be on or off
 */
void MAX_SetGates(uint8_t command)
{
    for (I2C_BUS_E n = I2C_Bus1; n < I2C_Bus_Count; n++)
    {
        HW_I2C_Master_Write_DMA(&devices[n], (uint8_t*)&command, 3);
    }
}

uint16_t MAX_ReadGates(void)
{
    uint16_t tmp;

    for (I2C_BUS_E n = I2C_Bus1; n < I2C_Bus_Count; n++)
    {
        tmp = tmp << 8;
        HW_I2C_Master_Write(&devices[n], (uint8_t*)&tmp, 1, MAX_BUS_TIMEOUT);
    }

    return tmp;
}
