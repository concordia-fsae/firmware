/**
 * @file HW_MPRL.c
 * @brief  Source code of MPRLS0300YG00001B driver
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-21
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_MPRL.h"

#include "HW_MAX7356.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    HW_I2C_Device_S dev;
    uint8_t dev_count;
} MPRL_Dev_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static MPRL_Dev_S devices[I2C_Bus_Count] = {
    [I2C_Bus1] = {
        .dev = {
            .handle = &i2c1,
            .addr = 0x18 << 1
        },
        .dev_count = MPRL_BUS1_COUNT
    },
    [I2C_Bus2] = {
        .dev = {
            .handle = &i2c2,
            .addr = 0x18 << 1
        },
        .dev_count = MPRL_BUS2_COUNT
    }
};


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Starts conversion of MPRL on both busses
 *
 * @note Assumes all gates are open
 */
void MPRL_StartConversion(void)
{
    const uint8_t command[] = { 0xaa, 0x00, 0x00 };

    for (int i = 0; i < 3; i++)
    {
        for (I2C_BUS_E n = I2C_Bus1; n < I2C_Bus_Count; n++)
        {
            HW_I2C_Master_Write_DMA(&devices[n].dev, (uint8_t*) &command, 3); 
        }
    }
}

/**
 * @brief  Get's MPRL status from bus
 *
 * @note Assumes the proper gate has been opened on the I2C multiplexer
 *
 * @param bus Bus to read from
 *
 * @retval   Status byte from MPRL
 */
uint8_t MPRL_ReadStatus(I2C_BUS_E bus)
{
    uint8_t stat;

    HW_I2C_Master_Read(&devices[bus].dev, &stat, 1, MPRL_BUS_TIMEOUT);

    return stat;
}

/**
 * @brief  Read data from 
 *
 * @note Assumes the proper gate has been opened on the I2C multiplexer
 *
 * @param bus Bus to read from
 *
 * @retval   Value returned by sensor
 */
uint32_t MPRL_ReadData(I2C_BUS_E bus)
{
    uint32_t data;

    HW_I2C_Master_Read(&devices[bus].dev, (uint8_t*) &data, 3, MPRL_BUS_TIMEOUT);

    return data >> 8;
}
