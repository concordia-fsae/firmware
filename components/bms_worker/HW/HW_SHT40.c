/**
 * @file HW_SHT40.c
 * @brief  Source code for SHT40 Driver
 *
 * @note Not used in Release A.1
 */

#if defined(BMSW_BOARD_VA3)

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW.h"
#include "HW_SHT40.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define READ_SENSOR_ID    0x89
#define HIGH_PRECISION_MEASURE 0xfd


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern HW_I2C_Handle_T i2c2;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

HW_I2C_Device_S I2C_SHT40 = {
    .addr   =  0x44,
    .handle = &i2c2,
};

SHT_S           sht_chip = {
    .dev = &I2C_SHT40,
};


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initializes SHT40 chip
 *
 * @retval true = Sucess, false = Failure
 */
bool SHT_init(void)
{
    uint8_t wdat    = READ_SENSOR_ID;
    uint8_t rdat[6] = { 0 };

    if (!HW_I2C_masterWrite(sht_chip.dev, &wdat, 1, 1))
    {
        sht_chip.state = SHT_ERROR;
        return false;
    }

    while (!HW_I2C_masterRead(sht_chip.dev, (uint8_t*)&rdat, 6, 10));

    sht_chip.serial_number = ((uint32_t)rdat[0]) | ((uint32_t)rdat[1] << 8) | ((uint32_t)rdat[2] << 16) | ((uint32_t)rdat[3] << 24);

    sht_chip.state = SHT_WAITING;
    return true;
}

/**
 * @brief  Start rh and temp conversion
 *
 * @retval true = Success, false = Failure
 */
bool SHT_startConversion(void)
{
    if (sht_chip.state != SHT_WAITING) return false;
    
    uint8_t wdat    = HIGH_PRECISION_MEASURE;

    if (!HW_I2C_masterWrite(sht_chip.dev, &wdat, 1, 10))
    {
        return false;
    }

    sht_chip.state = SHT_MEASURING;

    return true;
}

/**
 * @brief  Get rh and temp data
 *
 * @retval true = Success, false = Failure
 */
bool SHT_getData(void)
{
    if (sht_chip.state != SHT_MEASURING) return false;
    
    if (!HW_I2C_masterRead(sht_chip.dev, (uint8_t*)&sht_chip.data.raw, 6, 10))
    {
        return false;
    }
    

    float32_t temp_tmp = sht_chip.data.raw & 0xffff;
    float32_t rh_tmp = sht_chip.data.raw & 0xffff000000 >> 24;

    sht_chip.data.temp = -45 + 175.0f*(temp_tmp/65535);
    sht_chip.data.rh = -6 + 125.0f*(rh_tmp/65535);
    
    sht_chip.state = SHT_WAITING;

    return true;
}

/**
 * @brief  Start the SHT40 heater to remove condensation
 *
 * @retval true = Success, false = Failure
 */
bool SHT_startHeater(void)
{
    // TODO: Implement
    return false;
}

#endif // if defined(BMSW_BOARD_VA3)
