/**
 ******************************************************************************
 * @file    spi.c
 * @brief   This file provides code for the configuration
 *          of the SPI instances.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "HW_spi.h"

// SPI_HandleTypeDef hspi1;
LL_SPI_InitTypeDef hspi1;

static void LL_SPI_GPIOInit(SPI_TypeDef* SPIx);
static void LL_SPI_GPIODeInit(SPI_TypeDef* SPIx);

/* SPI1 init function */
void MX_SPI1_Init(void)
{
    // hspi1.Instance               = SPI1;
    // hspi1.Init.Mode              = SPI_MODE_MASTER;
    // hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    // hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    // hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    // hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    // hspi1.Init.NSS               = SPI_NSS_SOFT;
    // hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    // hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    // hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    // hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    // hspi1.Init.CRCPolynomial     = 10;
    hspi1.Mode = LL_SPI_MODE_MASTER;
    hspi1.TransferDirection = LL_SPI_FULL_DUPLEX;
    hspi1.DataWidth = LL_SPI_DATAWIDTH_8BIT;
    hspi1.ClockPolarity = LL_SPI_POLARITY_LOW;
    hspi1.ClockPhase = LL_SPI_PHASE_1EDGE;
    hspi1.NSS = LL_SPI_NSS_SOFT;
    hspi1.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV8;
    hspi1.BitOrder =  LL_SPI_MSB_FIRST;
    hspi1.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
    hspi1.CRCPoly = 0x00;

    LL_SPI_GPIOInit(SPI1);

    if (LL_SPI_Init(SPI1, &hspi1) != SUCCESS)
    {
        Error_Handler();
    }
    // __HAL_SPI_ENABLE(&hspi1);
    LL_SPI_Enable(SPI1);
}

// void HAL_SPI_MspInit(SPI_HandleTypeDef* spiHandle)
static void LL_SPI_GPIOInit(SPI_TypeDef *SPIx)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    if (SPIx == SPI1)
    {
        /* SPI1 clock enable */
        __HAL_RCC_SPI1_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();
        /**SPI1 GPIO Configuration
          PA5     ------> SPI1_SCK
          PA6     ------> SPI1_MISO
          PA7     ------> SPI1_MOSI
          */
        GPIO_InitStruct.Pin   = GPIO_PIN_5 | GPIO_PIN_7;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        GPIO_InitStruct.Pin  = GPIO_PIN_6;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        // __HAL_AFIO_REMAP_SPI1_ENABLE(); // use other pins for SPI1
        HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);
    }
}

// void HAL_SPI_MspDeInit(SPI_HandleTypeDef* spiHandle)
static inline void LL_SPI_GPIODeInit(SPI_TypeDef *SPIx)
{
    if (SPIx == SPI1)
    {
        /* Peripheral clock disable */
        LL_SPI_Disable(SPI1);

        /**SPI1 GPIO Configuration
          PA5     ------> SPI1_SCK
          PA6     ------> SPI1_MISO
          PA7     ------> SPI1_MOSI
          */
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7);
    }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
