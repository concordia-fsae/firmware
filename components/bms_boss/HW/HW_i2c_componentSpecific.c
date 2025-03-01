/**
 * @file HW_i2c_componentSpecific.c
 * @brief  Source code of I2C firmware/hardware interface
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW_i2c.h"
#include "HW.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

I2C_HandleTypeDef i2c;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initializes the I2C1 bus
 */
void HW_I2C_init(void)
{
    i2c.Instance             = I2C1;
    i2c.Init.ClockSpeed      = 400000U; /**< Clocked at 100kHz */
    i2c.Init.OwnAddress1     = 0;       /**< Translates to 0x12*/
    i2c.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    i2c.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    i2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    i2c.Init.OwnAddress2     = 0;
    i2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    i2c.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&i2c) == HAL_ERROR)
    {
        Error_Handler();
    }
}

/**
 * @brief  Msp call back for I2C initialization
 *
 * @param hi2c i2c handle being initialized
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
    /**< Activate clocks */
    __HAL_RCC_GPIOB_CLK_ENABLE();

    if (hi2c->Instance == I2C1)
    {
        __HAL_RCC_I2C1_CLK_ENABLE();
    }
}
