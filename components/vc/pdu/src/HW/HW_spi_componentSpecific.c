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
 *                           P U B L I C  V A R S
 ******************************************************************************/

const HW_spi_port_S HW_spi_ports[HW_SPI_PORT_COUNT] = {
    [HW_SPI_PORT_SPI1] = {
        .handle  = SPI1,
    },
};

const HW_SPI_Device_S HW_spi_devices[HW_SPI_DEV_COUNT] = {
    [HW_SPI_DEV_IMU] = {
        .port  = HW_SPI_PORT_SPI1,
        .ncs_pin = HW_GPIO_SPI_NCS_IMU,
    },
    [HW_SPI_DEV_SD] = {
        .port  = HW_SPI_PORT_SPI1,
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
HW_StatusTypeDef_E HW_SPI_init(void)
{
    LL_SPI_InitTypeDef hspi;

    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
    __HAL_AFIO_REMAP_SPI1_ENABLE();

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

    if (LL_SPI_Init(HW_spi_ports[HW_SPI_PORT_SPI1].handle, &hspi) != SUCCESS)
    {
        Error_Handler();
    }

    // enable SPI
    LL_SPI_Enable(HW_spi_ports[HW_SPI_PORT_SPI1].handle);

    return HW_OK;
}

/**
 * @brief Deinitializes the SPI peripheral
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_SPI_deInit(void)
{
    LL_SPI_Disable(HW_spi_ports[HW_SPI_PORT_SPI1].handle);
    LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_SPI1);

    return HW_OK;
}
