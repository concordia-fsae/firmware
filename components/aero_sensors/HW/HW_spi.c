/**
 * @file HW_spi.c
 * @brief  Source code of SPI hardware/firmware
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-13
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_spi.h"
#include "stm32f1xx_ll_spi.h"


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

SPI_TypeDef *spi2;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void HW_SPI_Init(void)
{
    LL_SPI_InitTypeDef SPI_Init        = { 0 };
    GPIO_InitTypeDef   GPIO_InitStruct = { 0 };

    /**< Enable Clock */
    __HAL_RCC_SPI2_CLK_ENABLE();

    spi2                       = SPI2;
    SPI_Init.Mode              = LL_SPI_MODE_MASTER;
    SPI_Init.TransferDirection = LL_SPI_FULL_DUPLEX;
    SPI_Init.DataWidth         = LL_SPI_DATAWIDTH_8BIT;
    SPI_Init.ClockPolarity     = LL_SPI_POLARITY_HIGH;
    SPI_Init.ClockPhase        = LL_SPI_PHASE_2EDGE;
    SPI_Init.NSS               = LL_SPI_NSS_SOFT;
    SPI_Init.BaudRate          = LL_SPI_BAUDRATEPRESCALER_DIV8; //LL_SPI_BAUDRATEPRESCALER_DIV64; //
    SPI_Init.BitOrder          = LL_SPI_MSB_FIRST;
    SPI_Init.CRCCalculation    = LL_SPI_CRCCALCULATION_DISABLE;
    SPI_Init.CRCPoly           = 0x00;

    /**
     * SPI2 GPIO Configuration
     * PB13 --> SPI2_SCK
     * PB14 --> SPI2_MISO
     * PB15 --> SPI_MOSI
     */

    GPIO_InitStruct.Pin   = SD_SCK2_Pin | SD_MOSI2_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = SD_MISO2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);


    if (LL_SPI_Init(spi2, &SPI_Init) != SUCCESS)
    {
        Error_Handler();
    }

    LL_SPI_Enable(SPI2);

}

inline uint8_t HW_SPI_Transmit8(SPI_TypeDef *hspi, uint8_t data)
{
    while (!LL_SPI_IsActiveFlag_TXE(hspi));
    LL_SPI_TransmitData8(hspi, data);
    while(!LL_SPI_IsActiveFlag_TXE(hspi));
    while(!LL_SPI_IsActiveFlag_RXNE(hspi));
    return LL_SPI_ReceiveData8(hspi); /**< Dummy read to clear RXNE and prevent OVR */
}

inline void HW_SPI_Transmit16(SPI_TypeDef *hspi, uint16_t data)
{
    HW_SPI_Transmit8(hspi, data);
    HW_SPI_Transmit8(hspi, data >> 8);
}

inline void HW_SPI_Transmit32(SPI_TypeDef *hspi, uint32_t data)
{
    HW_SPI_Transmit16(hspi, data);
    HW_SPI_Transmit16(hspi, data >> 16);
}

inline uint8_t HW_SPI_TransmitReceive8(SPI_TypeDef *hspi, uint8_t data)
{
    return HW_SPI_Transmit8(hspi, data);

}
