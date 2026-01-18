/**
 * @file HW_spi_componentSpecific.h
 * @brief  Header file for BMS Worker SPI firmware
 */

#pragma once

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    HW_SPI_PORT_SPI1 = 0x00U,
    HW_SPI_PORT_COUNT,
} HW_spi_port_E;

typedef enum
{
    HW_SPI_DEV_BMS = 0x00U,
    HW_SPI_DEV_COUNT,
} HW_spi_device_E;
