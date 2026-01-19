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
#include "stm32f1xx_ll_bus.h"


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

typedef struct
{
    bool            locked;
    HW_spi_device_E owner;
} HW_SPI_Lock_S;

static HW_SPI_Lock_S lock[HW_SPI_PORT_COUNT] = { 0 };

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

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Locks the SPI bus to a specific external peripheral
 *
 * @param dev SPI device to take ownerhsip of the bus
 *
 * @retval true = Device was able to lock, false = Failure
 */
bool HW_SPI_lock(HW_spi_device_E dev)
{
    taskENTER_CRITICAL();
    if (lock[HW_spi_devices[dev].port].locked)
    {
        return false;
    }

    lock[HW_spi_devices[dev].port].locked = true;
    lock[HW_spi_devices[dev].port].owner  = dev;
    HW_GPIO_writePin(HW_spi_devices[dev].ncs_pin, false);
    taskEXIT_CRITICAL();

    return true;
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
    lock[HW_spi_devices[dev].port].locked = false;

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
bool HW_SPI_transmit(HW_spi_device_E dev, uint8_t* data, uint8_t len)
{
    if (!verifyLock(dev))
    {
        return false;
    }

    for (uint8_t i = 0; i < len; i++)
    {
        while (!LL_SPI_IsActiveFlag_TXE(HW_spi_ports[HW_spi_devices[dev].port].handle))
        {
            ;
        }
        LL_SPI_TransmitData8(HW_spi_ports[HW_spi_devices[dev].port].handle, data[i]);
        while (!LL_SPI_IsActiveFlag_TXE(HW_spi_ports[HW_spi_devices[dev].port].handle))
        {
            ;
        }
        while (!LL_SPI_IsActiveFlag_RXNE(HW_spi_ports[HW_spi_devices[dev].port].handle))
        {
            ;
        }
        LL_SPI_ReceiveData8(HW_spi_ports[HW_spi_devices[dev].port].handle);    /**< Dummy read to clear RXNE and prevent OVR */
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
bool HW_SPI_transmitReceive(HW_spi_device_E dev, uint8_t* rwData, uint8_t len)
{
    if (!verifyLock(dev))
    {
        return false;
    }

    for (uint8_t i = 0; i < len; i++)
    {
        while (!LL_SPI_IsActiveFlag_TXE(HW_spi_ports[HW_spi_devices[dev].port].handle))
        {
            ;
        }
        LL_SPI_TransmitData8(HW_spi_ports[HW_spi_devices[dev].port].handle, rwData[i]);
        while (!LL_SPI_IsActiveFlag_TXE(HW_spi_ports[HW_spi_devices[dev].port].handle))
        {
            ;
        }
        while (!LL_SPI_IsActiveFlag_RXNE(HW_spi_ports[HW_spi_devices[dev].port].handle))
        {
            ;
        }
        rwData[i] = LL_SPI_ReceiveData8(HW_spi_ports[HW_spi_devices[dev].port].handle);
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
bool HW_SPI_transmitReceiveAsym(HW_spi_device_E dev, uint8_t* wData, uint8_t wLen, uint8_t* rData, uint8_t rLen)
{
    if (!verifyLock(dev))
    {
        return false;
    }

    HW_SPI_transmit(dev, wData, wLen);
    for (uint8_t i = 0; i < rLen; i++)
    {
        rData[i] = 0xff;
        HW_SPI_transmitReceive(dev, &rData[i], 1U);
    }

    return true;
}
