/**
 * HW_spi.c
 * Hardware SPI implementation
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "string.h"
#include "FreeRTOS.h"
#include "task.h"

// Firmware Includes
#include "HW_gpio.h"
#include "HW_spi.h"
#include "HW_dma.h"
#include "stm32f1xx_ll_bus.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CHECK_DMA(dma) (dma && (HAL_DMA_GetState(dma) == HAL_DMA_STATE_READY))
#define SPI_GET_PERIPH(dev) (&HW_spi_ports[HW_spi_devices[dev].port])

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

typedef struct
{
    HW_spi_device_E owner;
} HW_SPI_Lock_S;

static HW_SPI_Lock_S lock[HW_SPI_PORT_COUNT];

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * @brief  Verify if the SPI external peripheral has ownership of the SPI bus
 *
 * @param dev SPI external peripheral
 *
 * @retval true = Device is owner of bus, false = Failure
 */
static bool verifyLock(HW_spi_device_E dev)
{
    return lock[HW_spi_devices[dev].port].owner == dev;
}

static void dmaCompleteCB(DMA_HandleTypeDef *hdma)
{
    while (!LL_SPI_IsActiveFlag_TXE(SPI_GET_PERIPH((HW_spi_device_E)hdma->Parent)->handle));
    while (LL_SPI_IsActiveFlag_BSY(SPI_GET_PERIPH((HW_spi_device_E)hdma->Parent)->handle));
    while (LL_SPI_IsActiveFlag_RXNE(SPI_GET_PERIPH((HW_spi_device_E)hdma->Parent)->handle));

    LL_SPI_DisableDMAReq_RX(SPI_GET_PERIPH((HW_spi_device_E)hdma->Parent)->handle);
    LL_SPI_DisableDMAReq_TX(SPI_GET_PERIPH((HW_spi_device_E)hdma->Parent)->handle);

    HW_SPI_release((HW_spi_device_E)hdma->Parent);
}

static bool _dmaTransmitReceive(HW_spi_device_E dev, uint8_t* rwData, uint16_t len)
{
    bool started = HAL_DMA_Start_IT(SPI_GET_PERIPH(dev)->rx_dma, (uint32_t)&SPI_GET_PERIPH(dev)->handle->DR, (uint32_t)rwData, len) == HAL_OK;
    started &= HAL_DMA_Start_IT(SPI_GET_PERIPH(dev)->tx_dma, (uint32_t)rwData, (uint32_t)&SPI_GET_PERIPH(dev)->handle->DR, len) == HAL_OK;

    if (!started)
    {
        HAL_DMA_Abort_IT(SPI_GET_PERIPH(dev)->rx_dma);
        HAL_DMA_Abort_IT(SPI_GET_PERIPH(dev)->tx_dma);
        HW_SPI_release(dev);
        return false;
    }

    SPI_GET_PERIPH(dev)->rx_dma->XferCpltCallback = &dmaCompleteCB;
    SPI_GET_PERIPH(dev)->rx_dma->XferErrorCallback = &dmaCompleteCB;
    SPI_GET_PERIPH(dev)->rx_dma->Parent = (void*)dev;
    SPI_GET_PERIPH(dev)->tx_dma->Parent = (void*)dev;

    LL_SPI_EnableDMAReq_RX(SPI_GET_PERIPH(dev)->handle);
    LL_SPI_EnableDMAReq_TX(SPI_GET_PERIPH(dev)->handle);

    return true;
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

HW_StatusTypeDef_E  HW_SPI_init(void)
{
    for (uint8_t i = 0; i < HW_SPI_PORT_COUNT; i++)
    {
        lock[i].owner = HW_SPI_DEV_COUNT;
    }

    return HW_SPI_init_componentSpecific();
}

/**
 * @brief  Locks the SPI bus to a specific external peripheral
 *
 * @param dev SPI device to take ownerhsip of the bus
 *
 * @retval true = Device was able to lock, false = Failure
 */
bool HW_SPI_lock(HW_spi_device_E dev)
{
    bool locked = true;

    taskENTER_CRITICAL();
    if (lock[HW_spi_devices[dev].port].owner != HW_SPI_DEV_COUNT)
    {
        locked = false;
        goto out;
    }

    lock[HW_spi_devices[dev].port].owner = dev;
    HW_GPIO_writePin(HW_spi_devices[dev].ncs_pin, false);

out:
    taskEXIT_CRITICAL();

    return locked;
}

/**
 * @brief  Release bus ownership from device
 *
 * @param dev SPI external peripheral
 *
 * @retval true = Success, false = Failure
 */
bool HW_SPI_release(HW_spi_device_E dev)
{
    if (!verifyLock(dev))
    {
        return false;
    }

    HW_GPIO_writePin(HW_spi_devices[dev].ncs_pin, true);
    lock[HW_spi_devices[dev].port].owner  = HW_SPI_DEV_COUNT;

    return true;
}

/**
 * @note The bus must be under ownerhsip of the device
 *
 * @brief  Transmit 8 bits of data to peripheral
 *
 * @param dev SPI external peripherl
 * @param data Data to transmit
 * @param len Amount of sata to transmit
 *
 * @retval true = Success, false = Failure
 */
bool HW_SPI_transmit(HW_spi_device_E dev, uint8_t* data, uint16_t len)
{
    if (!verifyLock(dev))
    {
        return false;
    }

    for (uint8_t i = 0; i < len; i++)
    {
        while (!LL_SPI_IsActiveFlag_TXE(SPI_GET_PERIPH(dev)->handle));
        }
        LL_SPI_TransmitData8(SPI_GET_PERIPH(dev)->handle, data[i]);
        while (!LL_SPI_IsActiveFlag_TXE(SPI_GET_PERIPH(dev)->handle));
        while (!LL_SPI_IsActiveFlag_RXNE(SPI_GET_PERIPH(dev)->handle));
        LL_SPI_ReceiveData8(SPI_GET_PERIPH(dev)->handle);    /**< Dummy read to clear RXNE and prevent OVR */
    }

    return true;
}

/**
 * @note The bus must be under ownerhsip of the device
 *
 * @brief  Receive 8 bits of data to peripheral
 *
 * @param dev SPI external peripherl
 * @param data Data address to receive into
 * @param len Amount of sata to transmit
 *
 * @retval true = Success, false = Failure
 */
bool HW_SPI_receive(HW_spi_device_E dev, uint8_t* data, uint16_t len)
{
    if (!verifyLock(dev))
    {
        return false;
    }

    for (uint16_t i = 0; i < len; i++)
    {
        data[i] = 0xff;
        HW_SPI_transmitReceive(dev, &data[i], 1U);
    }

    return true;
}

/* @note The bus must be under ownerhsip of the device
 *
 * @brief  Transmit n bytes of data to peripheral
 *
 * @param dev SPI external peripherl
 * @param rwdata Data to transmit and where to write received data
 * @param len Amount of bytes to rw
 *
 * @retval true = Success, false = Failure
 */
bool HW_SPI_transmitReceive(HW_spi_device_E dev, uint8_t* rwData, uint16_t len)
{
    if (!verifyLock(dev))
    {
        return false;
    }

    for (uint8_t i = 0; i < len; i++)
    {
        while (!LL_SPI_IsActiveFlag_TXE(SPI_GET_PERIPH(dev)->handle));
        LL_SPI_TransmitData8(SPI_GET_PERIPH(dev)->handle, rwData[i]);
        while (!LL_SPI_IsActiveFlag_TXE(SPI_GET_PERIPH(dev)->handle));
        while (!LL_SPI_IsActiveFlag_RXNE(SPI_GET_PERIPH(dev)->handle));
        rwData[i] = LL_SPI_ReceiveData8(SPI_GET_PERIPH(dev)->handle);
    }

    return true;
}

/* @note The bus must be under ownerhsip of the device
 *
 * @brief  Transmit n bytes and receive m bytes of data to peripheral
 *
 * @param dev SPI external peripherl
 * @param wdata Data to transmit
 * @param wLen Amount of bytes to write
 * @param rdata Data to receive
 * @param rLen Amount of bytes to read
 *
 * @retval true = Success, false = Failure
 */
bool HW_SPI_transmitReceiveAsym(HW_spi_device_E dev, uint8_t* wData, uint16_t wLen, uint8_t* rData, uint16_t rLen)
{
    if (!verifyLock(dev))
    {
        return false;
    }

    HW_SPI_transmit(dev, wData, wLen);
    HW_SPI_receive(dev, rData, rLen);

    return true;
}

bool HW_SPI_dmaTransmitReceiveAsym(HW_spi_device_E dev, uint8_t* wData, uint16_t wLen, uint8_t* rData, uint16_t rLen)
{
    if (!CHECK_DMA(SPI_GET_PERIPH(dev)->rx_dma) || !CHECK_DMA(SPI_GET_PERIPH(dev)->tx_dma) || !HW_SPI_lock(dev))
    {
        HW_SPI_release(dev);
        return false;
    }

    HW_SPI_transmit(dev, wData, wLen);
    return _dmaTransmitReceive(dev, rData, rLen);
;
}

bool HW_SPI_dmaTransmitReceive(HW_spi_device_E dev, uint8_t* rwData, uint16_t len)
{
    if (!CHECK_DMA(SPI_GET_PERIPH(dev)->rx_dma) || !CHECK_DMA(SPI_GET_PERIPH(dev)->tx_dma) || !HW_SPI_lock(dev))
    {
        HW_SPI_release(dev);
        return false;
    }

    return _dmaTransmitReceive(dev, rwData, len);
}
