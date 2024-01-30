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
 */
void HW_I2C_Init(void)
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
 * @brief  Msp call back for I2C initialization
 *
 * @param hi2c i2c handle being initialized
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
    /**< Activate clocks */
    __HAL_RCC_GPIOB_CLK_ENABLE();

    if (hi2c->Instance == I2C2)
    {
        __HAL_RCC_I2C2_CLK_ENABLE();

        GPIO_InitTypeDef GPIO_InitStruct = { 0 };

        /**< Initialize I2C1 */
        GPIO_InitStruct.Pin   = I2C2_SCL_Pin | I2C2_SDA_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(I2C2_GPIO_Port, &GPIO_InitStruct);
    }
}

/**
 * @brief  Blocking mode write to I2C device
 *
 * @param dev I2C handle and address of device
 * @param pData Data buffer
 * @param Size Size (in bytes) to be transmitted
 * @param Timeout Maximum timeout
 */
bool HW_I2C_Master_Write(HW_I2C_Device_S* dev, uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
    return HAL_I2C_Master_Transmit(dev->handle, dev->addr << 1, pData, Size, Timeout) == HAL_OK;
}

/**
 * @brief  Blocking mode read from I2C device
 *
 * @param dev I2C handle and address of device
 * @param pData Data buffer
 * @param Size Size (in bytes) to be read
 * @param Timeout Maximum timeout
 */
bool HW_I2C_Master_Read(HW_I2C_Device_S* dev, uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
    return HAL_I2C_Master_Receive(dev->handle, dev->addr << 1, pData, Size, Timeout) == HAL_OK;
}

/**
 * @brief  Blocking mode read from I2C device register
 *
 * @param dev I2C handle and address of device
 * @param MemAddress Address to read from
 * @param MemAddSize Size of address
 * @param pData Pointer to data buffer
 * @param Size Size (in bytes) to be read
 * @param Timeout Maximum timeout allowed
 */
bool HW_I2C_Mem_Read(HW_I2C_Device_S* dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
    return HAL_I2C_Mem_Read(dev->handle, dev->addr << 1, MemAddress, MemAddSize, pData, Size, Timeout) == HAL_OK;
}


/**
 * @brief  Blocking mode write to I2C device register
 *
 * @param dev I2C handle and address of device
 * @param MemAddress Memaddress of I2C device to write to
 * @param MemAddSize Size of I2C device memory address
 * @param pData Pointer to data buffer
 * @param Size Size (in bytes) to be writen
 * @param Timeout Maximum allowed timeout
 */
bool HW_I2C_Mem_Write(HW_I2C_Device_S* dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
    return HAL_I2C_Mem_Write(dev->handle, dev->addr << 1, MemAddress, MemAddSize, pData, Size, Timeout) == HAL_OK;
}

/**
 * @brief  Non-blocking DMA write to I2C device
 *
 * @param dev I2C handle and address of device
 * @param pData Pointer to data buffer
 * @param Size Size (in bytes) to be written
 */
bool HW_I2C_Master_Write_DMA(HW_I2C_Device_S* dev, uint8_t* pData, uint16_t Size)
{
    return HAL_I2C_Master_Transmit_DMA(dev->handle, dev->addr << 1, pData, Size) == HAL_OK;
}

/**
 * @brief  Non-blocking read from I2C device
 *
 * @param dev I2C handle and address of device
 * @param pData Pointer to data
 * @param Size Size (in bytes) of data
 */
bool HW_I2C_Master_Read_DMA(HW_I2C_Device_S* dev, uint8_t* pData, uint16_t Size)
{
    return HAL_I2C_Master_Receive_DMA(dev->handle, dev->addr << 1, pData, Size) == HAL_OK;
}

/**
 * @brief  Non-blocking read from I2C device register
 *
 * @param dev I2C handle and address of device
 * @param MemAddress Address of I2C device to read from
 * @param MemAddSize Size of device address
 * @param pData Pointer to data buffer
 * @param Size Size (in bytes) to be read
 */
bool HW_I2C_Mem_Read_DMA(HW_I2C_Device_S* dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size)
{
    return HAL_I2C_Mem_Read_DMA(dev->handle, dev->addr << 1, MemAddress, MemAddSize, pData, Size) == HAL_OK;
}

/**
 * @brief  Non-blocking write to I2C register
 *
 * @param dev I2C handle and address of device
 * @param MemAddress Device register address to write too
 * @param MemAddSize Device register size
 * @param pData Pointer to data buffer
 * @param Size Size (in bytes) to be written
 */
bool HW_I2C_Mem_Write_DMA(HW_I2C_Device_S* dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size)
{
    return HAL_I2C_Mem_Write_DMA(dev->handle, dev->addr << 1, MemAddress, MemAddSize, pData, Size) == HAL_OK;
}
