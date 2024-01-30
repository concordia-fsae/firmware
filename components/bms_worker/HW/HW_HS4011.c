/**
 * @file HW_HS4011.c
 * @brief  Source file for HS4011 Relative Humidty/Temperature Sensor
 *
 * @note This File is only for Release A.1. Subsequent versions replaced this device.
 */

#if defined(BMSW_BOARD_VA1)

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
# include "ErrorHandler.h"
# include "Utility.h"
# include <stdint.h>

// Firmware Includes
# include "HW_HS4011.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

# define READ_SENSOR_ID   0xD7
# define NOHOLD_RH_T_MEAS 0xF5


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern HW_I2C_Handle_T i2c2;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

HW_I2C_Device_S I2C_HS4011 = {
    .addr   = 0x54,
    .handle = &i2c2,
};

HS4011_S hs_chip = {
    .dev = &I2C_HS4011,
};


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initializes HS4011 Driver
 *
 * @retval true = Success, false = Failure
 */
bool HS4011_Init(void)
{
    uint8_t wdat    = READ_SENSOR_ID;
    uint8_t rdat[4] = { 0 };

    if (!HW_I2C_Master_Write(hs_chip.dev, &wdat, 1, 1000))
    {
        // Error_Handler();
    }

    if (!HW_I2C_Master_Read(hs_chip.dev, (uint8_t*)&rdat, 4, 1000))
    {
        // Error_Handler();
    }

    hs_chip.serial_number = (uint32_t)*reverse_bytes((uint8_t*)&rdat, 4);

    return true;
}

/**
 * @brief  Start sensor conversion
 *
 * @retval  true = Success, false = Failure
 */
bool HS4011_StartConversion(void)
{
    uint8_t wdata = NOHOLD_RH_T_MEAS;

    if (hs_chip.data.measuring)
    {
        return false;
    }

    hs_chip.data.measuring = true;

    return HW_I2C_Master_Write(hs_chip.dev, &wdata, 1, 100);
}


/**
 * @brief  Communicate with sensor and retreive data
 *
 * @retval true = Data Received, false = Failure
 */
bool HS4011_GetData(void)
{
    uint8_t rdata[5] = { 0 };

    if (!HW_I2C_Master_Read(hs_chip.dev, (uint8_t*)&rdata, 5, 100))
    {
        return false;
    }

    hs_chip.data.measuring = false;

    reverse_bytes((uint8_t*)&rdata[0], 2);
    reverse_bytes((uint8_t*)&rdata[2], 2);

    // Formulas for the HS4011 specified at:
    // https://www.renesas.com/us/en/document/dst/hs40xx-datasheet?r=1575091
    // rh = (humidity_cnt / (2^14 - 1) * 100 converted to 0.01%
    // temp = (temp_cnt / (2^14 - 1) * 165 - 40 converted to 0.1 deg C
    hs_chip.data.rh   = (uint16_t)(((((uint32_t)rdata[1] << 8) | ((uint32_t)rdata[0])) * 10000) / 16383);
    hs_chip.data.temp = (int16_t)(((((int32_t)rdata[3] << 8) | ((int32_t)rdata[2])) * 1650) / 16383) - 400;

    return true;
}
#endif /**< BMSW_BOARD_VA1 */
