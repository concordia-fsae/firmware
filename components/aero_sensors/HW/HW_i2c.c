/**
  * @file HW_i2c.c
  * @brief  Source code of I2C firmware/hardware interface
  * @author Joshua Lafleur (josh.lafleur@outlook.com)
  * @version 0.1
  * @date 2022-07-09
  */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"

#include "HW_i2c.h"


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

I2C_HandleTypeDef i2c1;
I2C_HandleTypeDef i2c2;
DMA_HandleTypeDef hdma_i2c1_rx;
DMA_HandleTypeDef hdma_i2c1_tx;
DMA_HandleTypeDef hdma_i2c2_rx;
DMA_HandleTypeDef hdma_i2c2_tx;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/


/**
 * @brief  Initializes the I2C1 bus
 */
void HW_I2C1_Init(void)
{
    /**< Initialize peripherals */
    i2c1.Instance            = I2C1;
    i2c1.Init.ClockSpeed     = 100000U; /**< Clocked at 100kHz */
    i2c1.Init.OwnAddress1    = 0x08;    /**< Translates to 0x10*/
    i2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    if (HAL_I2C_Init(&i2c1) == HAL_ERROR)
    {
        Error_Handler();
    }
    
    i2c1.Instance            = I2C2;
    i2c1.Init.ClockSpeed     = 100000U; /**< Clocked at 100kHz */
    i2c1.Init.OwnAddress1    = 0x09;    /**< Translates to 0x11*/
    i2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
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
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    if (hi2c->Instance == I2C1)
    {
        /**
         * I2C1 Rx DMA configured on DMA Channel 2
         */
        hdma_i2c1_rx.Instance                 = DMA1_Channel2;
        hdma_i2c1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_i2c1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_i2c1_rx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_i2c1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_i2c1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_i2c1_rx.Init.Mode                = DMA_NORMAL;
        hdma_i2c1_rx.Init.Priority            = DMA_PRIORITY_MEDIUM;
        if (HAL_DMA_Init(&hdma_i2c1_rx) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_LINKDMA(hi2c, hdmarx, hdma_i2c1_rx);

        /**
         * I2C1 Tx DMA configured on DMA Channel 3
         */
        hdma_i2c1_tx.Instance                 = DMA1_Channel3;
        hdma_i2c1_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_i2c1_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_i2c1_tx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_i2c1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_i2c1_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_i2c1_tx.Init.Mode                = DMA_NORMAL;
        hdma_i2c1_tx.Init.Priority            = DMA_PRIORITY_MEDIUM;
        if (HAL_DMA_Init(&hdma_i2c1_tx) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_LINKDMA(hi2c, hdmatx, hdma_i2c1_tx);

    } else if (hi2c->Instance == I2C2)
    {
        /**
         * I2C1 Rx DMA configured on DMA Channel 2
         */
        hdma_i2c1_rx.Instance                 = DMA1_Channel3;
        hdma_i2c1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_i2c1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_i2c1_rx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_i2c1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_i2c1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_i2c1_rx.Init.Mode                = DMA_NORMAL;
        hdma_i2c1_rx.Init.Priority            = DMA_PRIORITY_MEDIUM;
        if (HAL_DMA_Init(&hdma_i2c2_rx) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_LINKDMA(hi2c, hdmarx, hdma_i2c2_rx);

        /**
         * I2C1 Tx DMA configured on DMA Channel 3
         */
        hdma_i2c1_tx.Instance                 = DMA1_Channel5;
        hdma_i2c1_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_i2c1_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_i2c1_tx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_i2c1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_i2c1_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_i2c1_tx.Init.Mode                = DMA_NORMAL;
        hdma_i2c1_tx.Init.Priority            = DMA_PRIORITY_MEDIUM;
        if (HAL_DMA_Init(&hdma_i2c2_tx) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_LINKDMA(hi2c, hdmatx, hdma_i2c2_tx);

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
void HW_I2C_Master_Write(HW_I2C_Device_S *dev, uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
    HAL_I2C_Master_Transmit(dev->handle, dev->addr, pData, Size, Timeout);
}

/**
 * @brief  Blocking mode read from I2C device
 *
 * @param dev I2C handle and address of device
 * @param pData Data buffer
 * @param Size Size (in bytes) to be read
 * @param Timeout Maximum timeout
 */
void HW_I2C_Master_Read(HW_I2C_Device_S *dev, uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
    HAL_I2C_Master_Receive(dev->handle, dev->addr, pData, Size, Timeout);
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
void HW_I2C_Mem_Read(HW_I2C_Device_S *dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
    HAL_I2C_Mem_Read(dev->handle, dev->addr, MemAddress, MemAddSize, pData, Size, Timeout);
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
void HW_I2C_Mem_Write(HW_I2C_Device_S *dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
    HAL_I2C_Mem_Write(dev->handle, dev->addr, MemAddress, MemAddSize, pData, Size, Timeout);
}

/**
 * @brief  Non-blocking DMA write to I2C device
 *
 * @param dev I2C handle and address of device
 * @param pData Pointer to data buffer
 * @param Size Size (in bytes) to be written
 */
void HW_I2C_Master_Write_DMA(HW_I2C_Device_S *dev, uint8_t* pData, uint16_t Size)
{
    HAL_I2C_Master_Transmit_DMA(dev->handle, dev->addr, pData, Size);
}

/**
 * @brief  Non-blocking read from I2C device
 *
 * @param dev I2C handle and address of device
 * @param pData Pointer to data
 * @param Size Size (in bytes) of data
 */
void HW_I2C_Master_Read_DMA(HW_I2C_Device_S *dev, uint8_t* pData, uint16_t Size)
{
    HAL_I2C_Master_Receive_DMA(dev->handle, dev->addr, pData, Size);

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
void HW_I2C_Mem_Read_DMA(HW_I2C_Device_S *dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size)
{
    HAL_I2C_Mem_Read_DMA(dev->handle, dev->addr, MemAddress, MemAddSize, pData, Size);
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
void HW_I2C_Mem_Write_DMA(HW_I2C_Device_S *dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size)
{
    HAL_I2C_Mem_Write_DMA(dev->handle, dev->addr, MemAddress, MemAddSize, pData, Size); 
}
