/**
 * @file HW_i2c.c
 * @brief  Source code of I2C firmware/hardware interface
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "SystemConfig.h"

// Firmware Includes
#include "HW_i2c.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

I2C_HandleTypeDef i2c2;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initializes the I2C1 bus
 *
 * @retval HW_OK
 */
void HW_I2C_init(void)
{
    i2c2.Instance             = I2C2;
    i2c2.Init.ClockSpeed      = 400000U; /**< Clocked at 100kHz */
    i2c2.Init.OwnAddress1     = 0x09;    /**< Translates to 0x12*/
    i2c2.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    i2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    i2c2.Init.OwnAddress2     = 0;
    i2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    i2c2.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&i2c2) == HAL_ERROR)
    {
        Error_Handler();
    }
}

/**
 * @brief  Deinitializes the I2C bus
 *
 * @retval HW_OK
 */
void HW_I2C_deInit(void)
{
	HAL_I2C_DeInit(&i2c2);
}


/**
 * @brief  Msp call back for I2C initialization
 *
 * @param hi2c i2c handle being initialized
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
    if (hi2c->Instance == I2C2)
    {
        __HAL_RCC_I2C2_CLK_ENABLE();
    }
}
