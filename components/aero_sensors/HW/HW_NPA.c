/**
 * @file HW_NPA.c
 * @brief  Source code of NPA-700B-030D driver
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-22
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_NPA.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define NPA_PRESSURE      0x00
#define NPA_TEMPERATURE8  0x01
#define NPA_TEMPERATURE16 0x02


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

HW_I2C_Device_S device = {
    .handle = &i2c1,
    .addr   = 0x28 << 1
};


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

inline uint32_t NPA_ReadRaw(uint8_t read_type);


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

inline uint32_t NPA_ReadRaw(uint8_t read_type)
{
    uint32_t tmp;

    switch (read_type)
    {
        case NPA_PRESSURE:
            HW_I2C_Master_Read(&device, (uint8_t*) &tmp, 2, NPA_BUS_TIMEOUT);
            break;
        case NPA_TEMPERATURE8:
            HW_I2C_Master_Read(&device, (uint8_t*) &tmp, 3, NPA_BUS_TIMEOUT);
            break;
        case NPA_TEMPERATURE16:
            HW_I2C_Master_Read(&device, (uint8_t*) &tmp, 4, NPA_BUS_TIMEOUT);
            break;
        default: /**< Should never reach the default case */
            tmp = 0;
            break;
    }

    return tmp;
}


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

NPA_Response_S NPA_Read(void)
{
    NPA_Response_S resp;
    uint32_t tmp = NPA_ReadRaw(NPA_PRESSURE);
    
    resp.status = (tmp & 0xc0000000) >> 30;
    resp.pressure = (tmp & 0x3fff0000) >> 16;
    
    return resp;
}

NPA_Full_Response8_S NPA_ReadTemp8(void)
{
    NPA_Full_Response8_S resp;
    uint32_t tmp = NPA_ReadRaw(NPA_TEMPERATURE8);
    
    resp.response.status = (tmp & 0xc0000000) >> 30;
    resp.response.pressure = (tmp & 0x3fff0000) >> 16;
    resp.temp = (tmp & 0x0000ff00) >> 8;

    return resp;
}

NPA_Full_Response16_S NPA_ReadTemp16(void)
{
    NPA_Full_Response16_S resp;
    uint32_t tmp = NPA_ReadRaw(NPA_TEMPERATURE16);
    
    resp.response.status = (tmp & 0xc0000000) >> 30;
    resp.response.pressure = (tmp & 0x3fff0000) >> 16;
    resp.temp = (tmp & 0x0000ffe0) >> 5;

    return resp;
}
