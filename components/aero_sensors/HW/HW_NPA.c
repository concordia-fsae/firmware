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
 *                         P R I V A T E  V A R S
 ******************************************************************************/

HW_I2C_Device_S device = {
    .handle = &i2c1,
    .addr   = 0x28 << 1
};


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

NPA_Response_S NPA_Read(void)
{
    NPA_Response_S resp;
    uint32_t tmp;
    
#if NPA_USE_TEMPERATURE == DONT_RECORD_TEMPURATURE
    HW_I2C_Master_Read(&device, (uint8_t*) &tmp, 2, NPA_BUS_TIMEOUT);
#elif NPA_USE_TEMPERATURE == RECORD_8BIT_TEMPERATURE
    HW_I2C_Master_Read(&device, (uint8_t*) &tmp, 3, NPA_BUS_TIMEOUT);
#elif NPA_USE_TEMPERATURE == RECORD_12BIT_TEMPERATURE
    HW_I2C_Master_Read(&device, (uint8_t*) &tmp, 4, NPA_BUS_TIMEOUT);
#endif

    resp.status = (tmp & 0xc0000000) >> 30;
    resp.pressure = (tmp & 0x3fff0000) >> 16;

#if NPA_USE_TEMPERATURE == RECORD_8BIT_TEMPERATURE
    resp.temperature = (tmp & 0x0000ff00) >> 8;
#elif NPA_USE_TEMPERATURE == RECORD_12BIT_TEMPERATURE
    resp.temperature = (tmp & 0x0000ffe0) >> 5;
#endif
    
    return resp;
}
