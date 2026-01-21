/**
 * HW_spi.c
 * Hardware SPI implementation
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW_spi.h"
#include "HW_gpio.h"
#include "stm32f1xx_ll_bus.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

DMA_HandleTypeDef hdma_spi_tx, hdma_spi_rx;

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const HW_spi_port_S HW_spi_ports[HW_SPI_PORT_COUNT] = {
    [HW_SPI_PORT_SPI3] = {
        .handle  = SPI3,
        .rx_dma = &hdma_spi_rx,
        .tx_dma = &hdma_spi_tx,
    },
};

const HW_SPI_Device_S HW_spi_devices[HW_SPI_DEV_COUNT] = {
    [HW_SPI_DEV_IMU] = {
        .port  = HW_SPI_PORT_SPI3,
        .ncs_pin = HW_GPIO_SPI_NCS_IMU,
    },
    [HW_SPI_DEV_SD] = {
        .port  = HW_SPI_PORT_SPI3,
        .ncs_pin = HW_GPIO_SPI_NCS_SD,
    },
};

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Initializes the SPI peripheral
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_SPI_init_componentSpecific(void)
{
    LL_SPI_InitTypeDef hspi;

    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI3);

    hspi.Mode              = LL_SPI_MODE_MASTER;
    hspi.TransferDirection = LL_SPI_FULL_DUPLEX;
    hspi.DataWidth         = LL_SPI_DATAWIDTH_8BIT;
    hspi.ClockPolarity     = LL_SPI_POLARITY_LOW;
    hspi.ClockPhase        = LL_SPI_PHASE_1EDGE;
    hspi.NSS               = LL_SPI_NSS_SOFT;
    hspi.BaudRate          = LL_SPI_BAUDRATEPRESCALER_DIV8;
    hspi.BitOrder          = LL_SPI_MSB_FIRST;
    hspi.CRCCalculation    = LL_SPI_CRCCALCULATION_DISABLE;
    hspi.CRCPoly           = 0x00;

    if (LL_SPI_Init(HW_spi_ports[HW_SPI_PORT_SPI3].handle, &hspi) != SUCCESS)
    {
        Error_Handler();
    }

    hdma_spi_rx.Instance                 = DMA2_Channel1;
    hdma_spi_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_spi_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_spi_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_spi_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_spi_rx.Init.Mode                = DMA_NORMAL;
    hdma_spi_rx.Init.Priority            = DMA_PRIORITY_MEDIUM;
    if (HAL_DMA_Init(&hdma_spi_rx) != HAL_OK)
    {
        Error_Handler();
    }
    hdma_spi_tx.Instance                 = DMA2_Channel2;
    hdma_spi_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_spi_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_spi_tx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_spi_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_spi_tx.Init.Mode                = DMA_NORMAL;
    hdma_spi_tx.Init.Priority            = DMA_PRIORITY_MEDIUM;
    if (HAL_DMA_Init(&hdma_spi_tx) != HAL_OK)
    {
        Error_Handler();
    }

    // enable SPI
    LL_SPI_Enable(HW_spi_ports[HW_SPI_PORT_SPI3].handle);

    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, DMA_IRQ_PRIO, 0U);
    HAL_NVIC_EnableIRQ(DMA2_Channel1_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, DMA_IRQ_PRIO, 0U);
    HAL_NVIC_EnableIRQ(DMA2_Channel2_IRQn);

    return HW_OK;
}

/**
 * @brief Deinitializes the SPI peripheral
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_SPI_deInit(void)
{
    LL_SPI_Disable(HW_spi_ports[HW_SPI_PORT_SPI3].handle);
    LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_SPI3);

    return HW_OK;
}
