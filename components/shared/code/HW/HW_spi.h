/**
 * @file HW_spi.h
 * @brief  Header file for SPI LL firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "HW.h"
#include "stdbool.h"

// Firmware Includes
#include "HW_gpio.h"
#include "HW_spi_componentSpecific.h"
#include "stm32f1xx_ll_spi.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef SPI_TypeDef HW_SPI_Handle_T;

typedef struct
{
    HW_SPI_Handle_T* handle;
} HW_spi_port_S;

typedef struct
{
    HW_spi_port_E    port;
    HW_GPIO_pinmux_E ncs_pin;
} HW_SPI_Device_S;

extern const HW_spi_port_S   HW_spi_ports[HW_SPI_PORT_COUNT];
extern const HW_SPI_Device_S HW_spi_devices[HW_SPI_DEV_COUNT];

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HW_StatusTypeDef_E HW_SPI_init(void);
HW_StatusTypeDef_E HW_SPI_deInit(void);
bool HW_SPI_lock(HW_spi_device_E dev);
bool HW_SPI_release(HW_spi_device_E dev);
bool HW_SPI_transmit8(HW_spi_device_E dev, uint8_t data);
bool HW_SPI_transmitReceive8(HW_spi_device_E dev, uint8_t wdata, uint8_t* rdata);
