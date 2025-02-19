/**
 * @file DRV_HIH_componentSpecific.c
 * @brief  Source code for HIH driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "DRV_HIH.h"
#include "HW_i2c.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

HW_I2C_Device_S I2C_HIH = {
    .addr   =  0x27,
    .handle = &i2c,
};

DRV_HIH_S hih_chip = {
    .dev = &I2C_HIH,
};
