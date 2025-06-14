/**
* @file cockpitLights.c
* @brief Module source that manages IMD and BMS lights
*/

/******************************************************************************
*                             I N C L U D E S
******************************************************************************/

#include "cockpitLights.h"
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

static struct
{
    bool imdState;
    bool bmsState;
} cockpitLights_data;

/******************************************************************************
*                       P U B L I C  F U N C T I O N S
******************************************************************************/

CAN_digitalStatus_E cockpitLights_imd_getStateCAN(void)
{
    return cockpitLights_data.imdState ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF;
}

CAN_digitalStatus_E cockpitLights_bms_getStateCAN(void)
{
    return cockpitLights_data.bmsState ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF;
}

static void cockpitLights_init(void)
{
    cockpitLights_data.imdState = true;
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_IMD_LIGHT_EN, DRV_IO_ACTIVE);

    cockpitLights_data.bmsState = true;
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_BMS_LIGHT_EN, DRV_IO_ACTIVE);
}


static void cockpitLights_periodic_10Hz(void)
{
    CAN_digitalStatus_E can_imd_status;
    CAN_digitalStatus_E can_bms_status;
    const bool imd_light_valid = (CANRX_get_signal(VEH, BMSB_imdStatusMem, &can_imd_status) == CANRX_MESSAGE_VALID);
    const bool bms_light_valid = (CANRX_get_signal(VEH, BMSB_bmsStatusMem, &can_bms_status) == CANRX_MESSAGE_VALID);

    if (imd_light_valid == true && (can_imd_status == CAN_DIGITALSTATUS_ON))
    {
        cockpitLights_data.imdState = false;
    }
    else
    {
        cockpitLights_data.imdState = true;
    }

    if (bms_light_valid == true && (can_imd_status == CAN_DIGITALSTATUS_ON))
    {
        cockpitLights_data.bmsState = false;
    }
    else
    {
        cockpitLights_data.bmsState = true;
    }

    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_IMD_LIGHT_EN, cockpitLights_data.imdState ? DRV_IO_ACTIVE : DRV_IO_INACTIVE);
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_BMS_LIGHT_EN, cockpitLights_data.bmsState ? DRV_IO_ACTIVE : DRV_IO_INACTIVE);
}

/******************************************************************************
*                           P U B L I C  V A R S
******************************************************************************/

const ModuleDesc_S cockpitLights_desc =
{
    .moduleInit = &cockpitLights_init,
    .periodic10Hz_CLK = &cockpitLights_periodic_10Hz,
};
