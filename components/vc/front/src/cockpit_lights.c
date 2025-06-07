/**
 * @file cockpit_lights.c
 * @brief Module source that manages IMD and BMS lights 
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/


#include "cockpit_lights.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "string.h"
#include "drv_outputAD.h"
#include "app_vehicleState.h"
 
 
 /******************************************************************************
  *                       P U B L I C  F U N C T I O N S
  ******************************************************************************/

 IMD_LIGHT_state_E IMD_LIGHT_getState(void)
 {
    return IMD_LIGHT_data.state;
 }
 
 CAN_digitalStatus_E IMD_LIGHT_getStateCAN(void)
 {
    return (IMD_LIGHT_data.state == IMD_LIGHT_ON) ?CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF;
 }
 
 static void IMD_LIGHT_init(void)
 {
   IMD_LIGHT_data.state = IMD_LIGHT_OFF;
 }

 BMS_LIGHT_state_E BMS_LIGHT_getState(void)
 {
    return BMS_LIGHT_data.state;
 }
 
 CAN_digitalStatus_E BMS_LIGHT_getStateCAN(void)
 {
    return (BMS_LIGHT_data.state == BMS_LIGHT_ON) ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF;
 }
 
 static void BMS_LIGHT_init(void)
 {
    BMS_LIGHT_data.state = BMS_LIGHT_OFF; 
 }
 
 static void BMS_LIGHT_periodic_10Hz(void)

 const bool chg_valid = (CANRX_get_signal(VEH, bmsb_bmsStatusMem, &IMD_LIGHT_state_E) == CANRX_MESSAGE_VALID);

 switch (IMD_LIGHT_state_E)
 {
     case CAN_DIGITALSTATUS_ON:
         drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_IMD_LIGHT_EN, DRV_IO_INACTIVE); 
         break;
     case CAN_DIGITALSTATUS_OFF:
         drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_IMD_LIGHT_EN, DRV_IO_ACTIVE);
         break; 
     default:
         drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_IMD_LIGHT_EN, DRV_IO_INACTIVE);
         break;
 }
}

const bool chg_valid = (CANRX_get_signal(VEH, bmsb_imdStatusMem, &BMS_LIGHT_state_E) == CANRX_MESSAGE_VALID);

switch (BMS_LIGHT_state_E) 
{
    case CAN_DIGITALSTATUS_ON:
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_BMS_LIGHT_EN, DRV_IO_INACTIVE); 
        break;
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
 
 
 const ModuleDesc_S cockpit_light_desc = {
    .moduleInit = &cockpit_light_init,
    .periodic10Hz_CLK = &cockpit_light_periodic_10Hz,
};