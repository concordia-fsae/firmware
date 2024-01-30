/**
 * @file HW_spi.h
 * @brief  Header file for SPI LL firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "SystemConfig.h"
#include "stdbool.h"

// Firmware Includes
#include "HW_gpio.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_ll_spi.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef SPI_TypeDef HW_SPI_Handle_T;

typedef struct
{
    HW_SPI_Handle_T* handle;
    HW_GPIO_S        ncs_pin;
} HW_SPI_Device_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void HW_SPI_Init(void);
bool HW_SPI_Lock(HW_SPI_Device_S* dev);
bool HW_SPI_Release(HW_SPI_Device_S* dev);

bool HW_SPI_Transmit8(HW_SPI_Device_S* dev, uint8_t data);
bool HW_SPI_Transmit16(HW_SPI_Device_S* dev, uint16_t data);
bool HW_SPI_Transmit32(HW_SPI_Device_S* dev, uint32_t data);
bool HW_SPI_TransmitReceive8(HW_SPI_Device_S* dev, uint8_t wdata, uint8_t* rdata);
