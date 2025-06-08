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
#include "drv_outputAD.c"
#include "drv_outputAD_componentSpecific.c"
#include "app_vehicleState.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/
struct
{
    IMD_LIGHT_state_E imd_state;
    BMS_LIGHT_state_E bms_state;
} cockpitLights_data;

/******************************************************************************
*                       P U B L I C  F U N C T I O N S
******************************************************************************/

IMD_LIGHT_state_E IMD_LIGHT_getState(void)
{
    return IMD_LIGHT_data.state;
    // shoudl i change to cockpitLights_data.imd_state?
}

BMS_LIGHT_state_E BMS_LIGHT_getState(void)
{
    return BMS_LIGHT_data.state;
}

CAN_digitalStatus_E cockpitLights_getStateCAN(void)
{
    return (IMD_LIGHT_data.state == IMD_LIGHT_ON) ?CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF;
    return (BMS_LIGHT_data.state == BMS_LIGHT_ON) ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF;
}

static void cockpitLight_init(void)
{
    IMD_LIGHT_data.state = IMD_LIGHT_OFF,
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_IMD_LIGHT_EN, DRV_IO_INACTIVE);
    //set pin to inactive, is this the correct syntax?;
    BMS_LIGHT_data.state = BMS_LIGHT_OFF,
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_BMS_LIGHT_EN, DRV_IO_INACTIVE);
    //set pin to inactive, is this the correct syntax?; 
}

BMS_LIGHT_state_E BMS_LIGHT_getState(void)
{
    return BMS_LIGHT_data.state;
}

static void cockpitLights_periodic_10Hz(void)
{
    const bool IMD_LIGHT_valid = (CANRX_get_signal(VEH, BMSB_bmsStatusMem, &cockpitLights_data.imd_state) == CANRX_MESSAGE_VALID);
// why it doesnt recognize CANRX_get_signal?
    switch (IMD_LIGHT_state_E)
    {
        case CAN_DIGITALSTATUS_ON:
            drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_IMD_LIGHT_EN, DRV_IO_INACTIVE); 
            break;// define f(x) DRV_OUTPUTAD_DIGITAL_IMD_LIGHT_EN IN CANIO_componentSpecific.h?
        case CAN_DIGITALSTATUS_OFF:
            drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_IMD_LIGHT_EN, DRV_IO_ACTIVE);
            break; 
        default:
            drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_IMD_LIGHT_EN, DRV_IO_INACTIVE);
            break;
    }

    const bool BMS_LIGHT_valid = (CANRX_get_signal(VEH, BMSB_imdStatusMem, &cockpitLights_data.bms_sate) == CANRX_MESSAGE_VALID);

    switch (BMS_LIGHT_state_E) 
    {
    case CAN_DIGITALSTATUS_ON:
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_BMS_LIGHT_EN, DRV_IO_INACTIVE); 
        break;// define f(x) DRV_OUTPUTAD_DIGITAL_BMS_LIGHT_EN IN CANIO_componentSpecific.h?
    case CAN_DIGITALSTATUS_OFF:
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_BMS_LIGHT_EN, DRV_IO_ACTIVE);
        break; 
    default:
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_BMS_LIGHT_EN, DRV_IO_INACTIVE);
        break;
    }
}

/******************************************************************************
*                           P U B L I C  V A R S
******************************************************************************/


const ModuleDesc_S cockpit_light_desc = 
{
    .moduleInit = &cockpitLight_init,
    .periodic10Hz_CLK = &cockpitLights_periodic_10Hz,
};