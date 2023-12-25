/**
 * HW_spi.h
 * Header file for the SPI hardware implementation
 */
#pragma once

#include "SystemConfig.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_ll_spi.h"

#include "stdbool.h"

#include "HW_gpio.h"

typedef SPI_TypeDef HW_SPI_Handle_T;

typedef struct {
    HW_SPI_Handle_T *handle;
    HW_GPIO_S ncs_pin;
} HW_SPI_Device_S;

void HW_SPI_Init(void);
bool HW_SPI_Lock(HW_SPI_Device_S*);
bool HW_SPI_Release(HW_SPI_Device_S*);

bool HW_SPI_Transmit8(HW_SPI_Device_S*, uint8_t);
bool HW_SPI_Transmit16(HW_SPI_Device_S*, uint16_t);
bool HW_SPI_Transmit32(HW_SPI_Device_S*, uint32_t);
bool HW_SPI_TransmitReceive8(HW_SPI_Device_S*, uint8_t, uint8_t*);
