/**
 * @file HW_HS4011.c
 * @brief  Source file for HS4011 Relative Humidty/Temperature Sensor
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version
 * @date 2023-12-19
 */

#if defined(BMSW_BOARD_VA1)

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_HS4011.h"

#include "HW_i2c.h"

#include "Utility.h"
#include "include/ErrorHandler.h"
#include <stdint.h>

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define READ_SENSOR_ID   0xD7
#define NOHOLD_RH_T_MEAS 0xF5


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern HW_I2C_Handle_T i2c2;


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

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
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

bool HS4011_Init()
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

bool HS4011_StartConversion()
{
    uint8_t wdata = NOHOLD_RH_T_MEAS;
    
    if (hs_chip.data.measuring)
    {
        return false;
    }

    hs_chip.data.measuring = true;

    return HW_I2C_Master_Write(hs_chip.dev, &wdata, 1, 100);
}

bool HS4011_GetData()
{
    uint8_t rdata[5] = { 0 };

    if (!HW_I2C_Master_Read(hs_chip.dev, (uint8_t*)&rdata, 5, 100))
    {
        return false;
    }

    hs_chip.data.measuring = false;

    reverse_bytes((uint8_t*)&rdata[0], 2);
    reverse_bytes((uint8_t*)&rdata[2], 2);

    // Formulas for the HS4011 specified at https://www.renesas.com/us/en/document/dst/hs40xx-datasheet?r=1575091
    // rh = (humidity_cnt / (2^14 - 1) * 100 converted to 0.01%
    // temp = (temp_cnt / (2^14 - 1) * 165 - 40 converted to 0.1 deg C
    hs_chip.data.rh   = (uint16_t)(((((uint32_t)rdata[1] << 8) | ((uint32_t)rdata[0])) * 10000) / 16383);
    hs_chip.data.temp = (int16_t)(((((int32_t)rdata[3] << 8) | ((int32_t)rdata[2])) * 1650) / 16383) - 400;
    
    return true;
}

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

#endif /**< BMSW_BOARD_VA1 */
