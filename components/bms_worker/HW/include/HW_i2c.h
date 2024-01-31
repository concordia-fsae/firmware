/**
 * @file HW_i2c.h
 * @brief  Header file for the I2C firmware/hardware interface
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "SystemConfig.h"
#include "stdbool.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef I2C_HandleTypeDef HW_I2C_Handle_T;

typedef struct
{
    HW_I2C_Handle_T* handle;
    uint16_t         addr;
} HW_I2C_Device_S;

typedef enum
{
    I2C_Bus1 = 0x00,
    I2C_Bus2,
    I2C_Bus_Count,
} I2C_BUS_E;


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

extern I2C_HandleTypeDef i2c2;
extern DMA_HandleTypeDef hdma_i2c2_rx;
extern DMA_HandleTypeDef hdma_i2c2_tx;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void HW_I2C_init(void);
bool HW_I2C_masterWrite(HW_I2C_Device_S* dev, uint8_t* pData, uint16_t Size, uint32_t Timeout);
bool HW_I2C_masterRead(HW_I2C_Device_S* dev, uint8_t* pData, uint16_t Size, uint32_t Timeout);
bool HW_I2C_memRead(HW_I2C_Device_S* dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size, uint32_t Timeout);
bool HW_I2C_memWrite(HW_I2C_Device_S* dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size, uint32_t Timeout);
bool HW_I2C_masterWriteDMA(HW_I2C_Device_S* dev, uint8_t* pData, uint16_t Size);
bool HW_I2C_masterReadDMA(HW_I2C_Device_S* dev, uint8_t* pData, uint16_t Size);
bool HW_I2C_memReadDMA(HW_I2C_Device_S* dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size);
bool HW_I2C_memWriteDMA(HW_I2C_Device_S* dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size);
