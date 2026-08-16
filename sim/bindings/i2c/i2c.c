#include "rig_runtime.h"

#include "HW_i2c.h"

I2C_HandleTypeDef i2c;

void HW_I2C_init(void)
{
}

void HW_I2C_deInit(void)
{
}

bool HW_I2C_masterWrite(HW_I2C_Device_S* dev, uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
    (void)dev;
    (void)pData;
    (void)Size;
    (void)Timeout;
    return true;
}

bool HW_I2C_masterRead(HW_I2C_Device_S* dev, uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
    (void)dev;
    (void)Timeout;
    for (uint16_t index = 0U; index < Size; index++)
    {
        pData[index] = 0U;
    }
    return true;
}

bool HW_I2C_memRead(HW_I2C_Device_S* dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
    (void)dev;
    (void)MemAddress;
    (void)MemAddSize;
    (void)Timeout;
    for (uint16_t index = 0U; index < Size; index++)
    {
        pData[index] = 0U;
    }
    return true;
}

bool HW_I2C_memWrite(HW_I2C_Device_S* dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
    (void)dev;
    (void)MemAddress;
    (void)MemAddSize;
    (void)pData;
    (void)Size;
    (void)Timeout;
    return true;
}

bool HW_I2C_masterWriteDMA(HW_I2C_Device_S* dev, uint8_t* pData, uint16_t Size)
{
    (void)dev;
    (void)pData;
    (void)Size;
    return true;
}

bool HW_I2C_masterReadDMA(HW_I2C_Device_S* dev, uint8_t* pData, uint16_t Size)
{
    (void)dev;
    for (uint16_t index = 0U; index < Size; index++)
    {
        pData[index] = 0U;
    }
    return true;
}

bool HW_I2C_memReadDMA(HW_I2C_Device_S* dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size)
{
    (void)dev;
    (void)MemAddress;
    (void)MemAddSize;
    for (uint16_t index = 0U; index < Size; index++)
    {
        pData[index] = 0U;
    }
    return true;
}

bool HW_I2C_memWriteDMA(HW_I2C_Device_S* dev, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size)
{
    (void)dev;
    (void)MemAddress;
    (void)MemAddSize;
    (void)pData;
    (void)Size;
    return true;
}
