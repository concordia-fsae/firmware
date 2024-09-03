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
#include "HW_spi.h"
#include "stm32f1xx_ll_bus.h"
#include "stm32f1xx_ll_gpio.h"


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

typedef struct
{
    bool            locked;
    HW_SPI_Device_S * owner;
} HW_SPI_Lock_S;

static HW_SPI_Lock_S      lock = { 0 };
static LL_SPI_InitTypeDef hspi1;


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static void LL_SPI_GPIOInit(SPI_TypeDef* SPIx);
static void LL_SPI_GPIODeInit(SPI_TypeDef* SPIx);

bool        HW_SPI_verifyLock(HW_SPI_Device_S* dev);


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
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    if (SPIx == SPI1)
    {
        // __HAL_RCC_SPI1_CLK_ENABLE();
        LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
        LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOB);

        GPIO_InitStruct.Pin   = SPI1_CLK_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(SPI1_GPIO_Port, &GPIO_InitStruct);

        GPIO_InitStruct.Pin   = SPI1_MOSI_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(SPI1_GPIO_Port, &GPIO_InitStruct);

        GPIO_InitStruct.Pin   = SPI1_MISO_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        HAL_GPIO_Init(SPI1_GPIO_Port, &GPIO_InitStruct);

        LL_GPIO_AF_EnableRemap_SPI1();

        __HAL_RCC_GPIOB_CLK_ENABLE();

        GPIO_InitStruct.Pin   = SPI1_MAX_NCS_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(SPI1_MAX_NCS_Port, &GPIO_InitStruct);

#if defined(BMSW_BOARD_VA1)
        GPIO_InitStruct.Pin   = SPI1_LTC_NCS_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(SPI1_LTC_NCS_Port, &GPIO_InitStruct);
#endif /**< BMSW_BOARD_VA1 */
        // set NSS pin high (disable slaves)
        HAL_GPIO_WritePin(SPI1_MAX_NCS_Port, SPI1_MAX_NCS_Pin, GPIO_PIN_SET);
#if defined(BMSW_BOARD_VA1)
        HAL_GPIO_WritePin(SPI1_LTC_NCS_Port, SPI1_LTC_NCS_Pin, GPIO_PIN_SET);
#endif /**< BMSW_BOARD_VA1 */
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

        HAL_GPIO_DeInit(SPI1_GPIO_Port,    SPI1_CLK_Pin | SPI1_MISO_Pin | SPI1_MOSI_Pin);
        HAL_GPIO_DeInit(SPI1_MAX_NCS_Port, SPI1_MAX_NCS_Pin);
#if defined(BMSW_BOARD_VA1)
        HAL_GPIO_DeInit(SPI1_LTC_NCS_Port, SPI1_LTC_NCS_Pin);
#endif /**, BMSW_BOARD_VA1 */
	
	    LL_GPIO_AF_DisableRemap_SPI1();

        HAL_GPIO_DeInit(SPI1_GPIO_Port, SPI1_CLK_Pin | SPI1_MISO_Pin |SPI1_MOSI_Pin);

	    LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_SPI1);
    }
}

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

    HAL_GPIO_WritePin(dev->ncs_pin.port, dev->ncs_pin.pin, GPIO_PIN_RESET);

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

    HAL_GPIO_WritePin(dev->ncs_pin.port, dev->ncs_pin.pin, GPIO_PIN_SET);

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
