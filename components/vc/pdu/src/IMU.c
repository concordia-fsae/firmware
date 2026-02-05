/**
* @file cockpitLights.c
* @brief Module source for IMU data 
*/

/******************************************************************************
*                             I N C L U D E S
******************************************************************************/

#include "IMU.h"
#include "CANIO_componentSpecific.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "string.h"
#include "drv_outputAD.h"
#include "app_vehicleState.h"
#include "MessageUnpack_generated.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static int reg_read(uint8_t reg, uint8_t *buf, uint16_t len);
static int reg_write(uint8_t reg, const uint8_t *buf, uint8_t len);

 /******************************************************************************
*                       P U B L I C  F U N C T I O N S
******************************************************************************/

imu_status_e imuInit(void){
    // imu_hw_delay_ms(50);
    uint8_t who = 0;
    if (imuReadWhoIAm(&who) != IMU_STATUS_OK){
        return IMU_STATUS_ERROR;
    }
    return IMU_STATUS_OK;    
}

imu_status_e imuReadWhoAmI(uint8_t *id){
    if (id==NULL){
        return IMU_STATUS_ERROR;
    }

    if (reg_read(0x0F, id, 1) != 0){
        return IMU_STATUS_ERROR;
    }

    if (*id != 0x6B)
    {
        return IMU_STATUS_NOT_FOUND;
    }

    if (*id == 0x6B){
        return IMU_STATUS_OK;
    }
}

imu_status_e imuReadData(imu_siData_s *data);{

}