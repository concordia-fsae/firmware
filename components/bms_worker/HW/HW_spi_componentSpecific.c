/**
 * HW_spi.c
 * Hardware SPI implementation
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "string.h"

// Firmware Includes
#include "HW_gpio.h"
#include "HW_spi.h"
#include "stm32f1xx_ll_bus.h"


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

static LL_SPI_InitTypeDef hspi1;

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static void LL_SPI_GPIOInit(SPI_TypeDef* SPIx);
static void LL_SPI_GPIODeInit(SPI_TypeDef* SPIx);

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Initializes the SPI peripheral
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_SPI_init(void)
{
    // initialize SPI pins
    LL_SPI_GPIOInit(SPI1);

    hspi1.Mode              = LL_SPI_MODE_MASTER;
    hspi1.TransferDirection = LL_SPI_FULL_DUPLEX;
    hspi1.DataWidth         = LL_SPI_DATAWIDTH_8BIT;
    hspi1.ClockPolarity     = LL_SPI_POLARITY_LOW;
    hspi1.ClockPhase        = LL_SPI_PHASE_1EDGE;
    hspi1.NSS               = LL_SPI_NSS_SOFT;
    hspi1.BaudRate          = LL_SPI_BAUDRATEPRESCALER_DIV128;
    hspi1.BitOrder          = LL_SPI_MSB_FIRST;
    hspi1.CRCCalculation    = LL_SPI_CRCCALCULATION_DISABLE;
    hspi1.CRCPoly           = 0x00;

    if (LL_SPI_Init(SPI1, &hspi1) != SUCCESS)
    {
        Error_Handler();
    }

    // enable SPI
    LL_SPI_Enable(SPI1);

    return HW_OK;
}

/**
 * @brief Deinitializes the SPI peripheral
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_SPI_deInit(void)
{
    LL_SPI_GPIODeInit(SPI1);

    return HW_OK;
}

/**
 * @brief  Initializes SPI pins
 *
 * @param SPIx SPI peripheral
 */
static void LL_SPI_GPIOInit(SPI_TypeDef* SPIx)
{
    if (SPIx == SPI1)
    {
        // __HAL_RCC_SPI1_CLK_ENABLE();
        LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
        LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOB);

	    __HAL_AFIO_REMAP_SPI1_ENABLE();
    }
}

/**
 * @brief  Deinitializes ll SPI peripheral
 *
 * @param SPIx SPI peripheral
 */
static void LL_SPI_GPIODeInit(SPI_TypeDef* SPIx)
{
    if (SPIx == SPI1)
    {
        // Peripheral clock disable
        LL_SPI_Disable(SPI1);
	    __HAL_AFIO_REMAP_SPI1_DISABLE();

	    LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_SPI1);
    }
}
