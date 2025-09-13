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

typedef struct
{
    bool            locked;
    HW_SPI_Device_S * owner;
} HW_SPI_Lock_S;

static HW_SPI_Lock_S      lock = { 0 };

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool        HW_SPI_verifyLock(HW_SPI_Device_S* dev);

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
bool HW_SPI_lock(HW_SPI_Device_S* dev)
{
    if (lock.locked)
    {
        return false;
    }

    lock.locked = true;
    lock.owner  = dev;

    HW_GPIO_writePin(dev->ncs_pin, false);

    return true;
}

/**
 * @brief  Release bus ownership from device
 *
 * @param dev SPI external peripheral
 *
 * @retval true = Success, false = Failure
 */
bool HW_SPI_release(HW_SPI_Device_S* dev)
{
    if (!HW_SPI_verifyLock(dev))
    {
        return false;
    }

    lock.locked = false;
    lock.owner  = 0x00;

    HW_GPIO_writePin(dev->ncs_pin, true);

    return true;
}

/**
 * @brief  Verify if the SPI external peripheral has ownership of the SPI bus
 *
 * @param dev SPI external peripheral
 *
 * @retval true = Device is owner of bus, false = Failure
 */
bool HW_SPI_verifyLock(HW_SPI_Device_S* dev)
{
    if (lock.owner != dev)
    {
        return false;
    }

    return true;
}

/**
 * @note The bus must be under ownerhsip of the device
 *
 * @brief  Transmit 8 bits of data to peripheral
 *
 * @param dev SPI external peripherl
 * @param data Data to transmit
 *
 * @retval true = Success, false = Failure
 */
bool HW_SPI_transmit8(HW_SPI_Device_S* dev, uint8_t data)
{
    if (lock.owner != dev)
    {
        return false;
    }

    while (!LL_SPI_IsActiveFlag_TXE(dev->handle))
    {
        ;
    }
    LL_SPI_TransmitData8(dev->handle, data);
    while (!LL_SPI_IsActiveFlag_TXE(dev->handle))
    {
        ;
    }
    while (!LL_SPI_IsActiveFlag_RXNE(dev->handle))
    {
        ;
    }
    LL_SPI_ReceiveData8(dev->handle);    /**< Dummy read to clear RXNE and prevent OVR */

    return true;
}

/* @note The bus must be under ownerhsip of the device
 *
 * @brief  Transmit 16 bits of data to peripheral
 *
 * @param dev SPI external peripherl
 * @param data Data to transmit
 *
 * @retval true = Success, false = Failure
 */
bool HW_SPI_transmit16(HW_SPI_Device_S* dev, uint16_t data)
{
    if (!HW_SPI_transmit8(dev, (uint8_t)(data >> 8)))
    {
        return false;
    }
    HW_SPI_transmit8(dev, (uint8_t)data);

    return true;
}

/* @note The bus must be under ownerhsip of the device
 *
 * @brief  Transmit 32 bits of data to peripheral
 *
 * @param dev SPI external peripherl
 * @param data Data to transmit
 *
 * @retval true = Success, false = Failure
 */
bool HW_SPI_transmit32(HW_SPI_Device_S* dev, uint32_t data)
{
    if (!HW_SPI_transmit16(dev, (uint16_t)(data >> 16)))
    {
        return false;
    }
    HW_SPI_transmit16(dev, (uint16_t)data);

    return true;
}

/* @note The bus must be under ownerhsip of the device
 *
 * @brief  Transmit 16 bits of data to peripheral
 *
 * @param dev SPI external peripherl
 * @param wdata Data to transmit
 * @param rdata Data to receive
 *
 * @retval true = Success, false = Failure
 */
bool HW_SPI_transmitReceive8(HW_SPI_Device_S* dev, uint8_t wdata, uint8_t* rdata)
{
    if (lock.owner != dev)
    {
        return false;
    }

    while (!LL_SPI_IsActiveFlag_TXE(dev->handle))
    {
        ;
    }
    LL_SPI_TransmitData8(dev->handle, wdata);
    while (!LL_SPI_IsActiveFlag_TXE(dev->handle))
    {
        ;
    }
    while (!LL_SPI_IsActiveFlag_RXNE(dev->handle))
    {
        ;
    }
    *rdata = LL_SPI_ReceiveData8(dev->handle);

    return true;
}
