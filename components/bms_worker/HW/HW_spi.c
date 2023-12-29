/**
 * HW_spi.c
 * Hardware SPI implementation
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_spi.h"
#include "SystemConfig.h"
#include "stm32f1xx_ll_bus.h"
#include "stm32f1xx_ll_gpio.h"

#include "string.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

typedef struct {
    bool locked;
    HW_SPI_Device_S* owner; 
} HW_SPI_Lock_S;

static HW_SPI_Lock_S lock = {0};
static LL_SPI_InitTypeDef hspi1;

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static void LL_SPI_GPIOInit(SPI_TypeDef* SPIx);
static void LL_SPI_GPIODeInit(SPI_TypeDef* SPIx);

bool HW_SPI_Verify(HW_SPI_Device_S*);

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * MX_SPI1_Init
 * Initialize the SPI peripheral
 */
void HW_SPI_Init(void)
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
        //__HAL_RCC_SPI1_CLK_ENABLE();
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

        GPIO_InitStruct.Pin  = SPI1_MISO_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(SPI1_GPIO_Port, &GPIO_InitStruct);

        LL_GPIO_AF_EnableRemap_SPI1();
       
        __HAL_RCC_GPIOB_CLK_ENABLE();

        GPIO_InitStruct.Pin  = SPI1_MAX_NCS_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(SPI1_MAX_NCS_Port, &GPIO_InitStruct);
         
        GPIO_InitStruct.Pin  = SPI1_LTC_NCS_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(SPI1_LTC_NCS_Port, &GPIO_InitStruct);

        // set NSS pin high (disable slaves)
        HAL_GPIO_WritePin(SPI1_MAX_NCS_Port, SPI1_MAX_NCS_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(SPI1_LTC_NCS_Port, SPI1_LTC_NCS_Pin, GPIO_PIN_SET);
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

        HAL_GPIO_DeInit(SPI1_GPIO_Port, SPI1_CLK_Pin | SPI1_MISO_Pin |SPI1_MOSI_Pin);
        HAL_GPIO_DeInit(SPI1_MAX_NCS_Port, SPI1_MAX_NCS_Pin);
        HAL_GPIO_DeInit(SPI1_LTC_NCS_Port, SPI1_LTC_NCS_Pin);
    }
}

bool HW_SPI_Lock(HW_SPI_Device_S* dev)
{
    if (lock.locked)
    {
        return false;
    }

    lock.locked = true;
    lock.owner = dev;

    HAL_GPIO_WritePin(dev->ncs_pin.port, dev->ncs_pin.pin, GPIO_PIN_RESET);

    return true;
}

bool HW_SPI_Release(HW_SPI_Device_S* dev)
{
    if (!HW_SPI_Verify(dev))
    {
        return false;
    }

    lock.locked = false;
    lock.owner = 0x00;

    HAL_GPIO_WritePin(dev->ncs_pin.port, dev->ncs_pin.pin, GPIO_PIN_SET);

    return true;
}

bool HW_SPI_Verify(HW_SPI_Device_S *dev)
{
    if (lock.owner != dev)
    {
        return false;
    }

    return true;
}

bool HW_SPI_Transmit8(HW_SPI_Device_S *dev, uint8_t data)
{
    if (lock.owner != dev)
    {
        return false;
    }
    
    while (!LL_SPI_IsActiveFlag_TXE(dev->handle));
    LL_SPI_TransmitData8(dev->handle, data);
    while(!LL_SPI_IsActiveFlag_TXE(dev->handle));
    while(!LL_SPI_IsActiveFlag_RXNE(dev->handle));
    LL_SPI_ReceiveData8(dev->handle); /**< Dummy read to clear RXNE and prevent OVR */
    
    return true;
}

bool HW_SPI_Transmit16(HW_SPI_Device_S *dev, uint16_t data)
{
    if (!HW_SPI_Transmit8(dev, data >> 8))
    {
        return false;
    }
    HW_SPI_Transmit8(dev, data);
    
    return true;
}

bool HW_SPI_Transmit32(HW_SPI_Device_S *dev, uint32_t data)
{
    if (!HW_SPI_Transmit16(dev, data >> 16))
    {
        return false;
    }
    HW_SPI_Transmit16(dev, data);

    return true;
}

bool HW_SPI_TransmitReceive8(HW_SPI_Device_S *dev, uint8_t wdata, uint8_t *rdata)
{
    if (lock.owner != dev)
    {
        return false;
    }
    
    while (!LL_SPI_IsActiveFlag_TXE(dev->handle));
    LL_SPI_TransmitData8(dev->handle, wdata);
    while(!LL_SPI_IsActiveFlag_TXE(dev->handle));
    while(!LL_SPI_IsActiveFlag_RXNE(dev->handle));
    *rdata = LL_SPI_ReceiveData8(dev->handle);

    return true;
}
