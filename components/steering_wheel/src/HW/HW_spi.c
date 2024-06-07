/**
 * HW_spi.c
 * Hardware SPI implementation
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_spi.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

LL_SPI_InitTypeDef hspi1;

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static void LL_SPI_GPIOInit(SPI_TypeDef* SPIx);
static void LL_SPI_GPIODeInit(SPI_TypeDef* SPIx);

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * MX_SPI1_Init
 * Initialize the SPI peripheral
 */
void MX_SPI1_Init(void)
{
    hspi1.Mode              = LL_SPI_MODE_MASTER;
    hspi1.TransferDirection = LL_SPI_FULL_DUPLEX;
    hspi1.DataWidth         = LL_SPI_DATAWIDTH_8BIT;
    hspi1.ClockPolarity     = LL_SPI_POLARITY_LOW;
    hspi1.ClockPhase        = LL_SPI_PHASE_1EDGE;
    hspi1.NSS               = LL_SPI_NSS_SOFT;
    hspi1.BaudRate          = LL_SPI_BAUDRATEPRESCALER_DIV8;
    hspi1.BitOrder          = LL_SPI_MSB_FIRST;
    hspi1.CRCCalculation    = LL_SPI_CRCCALCULATION_DISABLE;
    hspi1.CRCPoly           = 0x00;

    // initialize SPI pins
    LL_SPI_GPIOInit(SPI1);

    if (LL_SPI_Init(SPI1, &hspi1) != SUCCESS)
    {
        Error_Handler();
    }

    // enable SPI
    LL_SPI_Enable(SPI1);
}

/**
 * LL_SPI_GPIOInit
 * @param SPIx SPI handle to operate on
 */
static void LL_SPI_GPIOInit(SPI_TypeDef *SPIx)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    if (SPIx == SPI1)
    {
        // enable SPI clock
        __HAL_RCC_SPI1_CLK_ENABLE();

        // SPI1 GPIO Configuration
        // PA5     ------> SPI1_SCK
        // PA6     ------> SPI1_MISO
        // PA7     ------> SPI1_MOSI

        GPIO_InitStruct.Pin   = GPIO_PIN_5 | GPIO_PIN_7;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        GPIO_InitStruct.Pin  = GPIO_PIN_6;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        // set NSS pin high (disable slaves)
        HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);
    }
}

/**
 * LL_SPI_GPIODeInit
 * @param SPIx SPI handle to operate on
 */
static inline void LL_SPI_GPIODeInit(SPI_TypeDef *SPIx)
{
    if (SPIx == SPI1)
    {
        // Peripheral clock disable
        LL_SPI_Disable(SPI1);

        // SPI1 GPIO Configuration
        // PA5     ------> SPI1_SCK
        // PA6     ------> SPI1_MISO
        // PA7     ------> SPI1_MOSI
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7);
    }
}
