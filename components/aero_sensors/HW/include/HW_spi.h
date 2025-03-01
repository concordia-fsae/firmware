/**
 * @file HW_spi.h
 * @brief  Header file for SPI firmware/hardware
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-13
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

extern SPI_TypeDef *spi2;


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void HW_SPI_Init(void);
uint8_t HW_SPI_Transmit8(SPI_TypeDef *hspi, uint8_t data);
void HW_SPI_Transmit16(SPI_TypeDef *hspi, uint16_t data);
void HW_SPI_Transmit32(SPI_TypeDef *hspi, uint32_t data);
uint8_t HW_SPI_TransmitReceive8(SPI_TypeDef *hspi, uint8_t data);
