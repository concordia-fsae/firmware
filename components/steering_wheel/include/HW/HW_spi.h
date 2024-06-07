/**
 * HW_spi.h
 * Header file for the SPI hardware implementation
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Includes ------------------------------------------------------------------
#include "SystemConfig.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_ll_spi.h"

// extern SPI_HandleTypeDef hspi1;
extern LL_SPI_InitTypeDef hspi1;

void MX_SPI1_Init(void);

#ifdef __cplusplus
}
#endif
