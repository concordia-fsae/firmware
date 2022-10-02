/**
 * @file HW_i2c.h
 * @brief  Header file for the I2C firmware/hardware interface
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-09
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef I2C_HandleTypeDef HW_I2C_Handle_T;

typedef struct
{
    HW_I2C_Handle_T *handle;
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

extern I2C_HandleTypeDef i2c1;
extern DMA_HandleTypeDef hdma_i2c1_rx;
extern DMA_HandleTypeDef hdma_i2c1_tx;
extern I2C_HandleTypeDef i2c2;
extern DMA_HandleTypeDef hdma_i2c2_rx;
extern DMA_HandleTypeDef hdma_i2c2_tx;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void HW_I2C_Init(void);
void HW_I2C_Master_Write(HW_I2C_Device_S *dev, uint8_t* pData, uint16_t Size, uint32_t Timeout);
void HW_I2C_Master_Read(HW_I2C_Device_S *dev, uint8_t* pData, uint16_t Size, uint32_t Timeout);
void HW_I2C_Mem_Read(HW_I2C_Device_S *dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size, uint32_t Timeout);
void HW_I2C_Mem_Write(HW_I2C_Device_S *dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size, uint32_t Timeout);
void HW_I2C_Master_Write_DMA(HW_I2C_Device_S *dev, uint8_t* pData, uint16_t Size);
void HW_I2C_Master_Read_DMA(HW_I2C_Device_S *dev, uint8_t* pData, uint16_t Size);
void HW_I2C_Mem_Read_DMA(HW_I2C_Device_S *dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size);
void HW_I2C_Mem_Write_DMA(HW_I2C_Device_S *dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size);
#if defined(I2C2_TEST) || defined(I2C1_TEST)
void HW_I2C_Test();
#endif /**< I2C1_TEST || I2C2_TEST */

