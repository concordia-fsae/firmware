/**
 * @file tssibringup.c
 * @brief Module for Tractive System Status Indicator (TSSI) Lights Based on Status from the Insulation Monitoring Device (IMD).
 */

#include "tssi.h"
// #include "HW_gpio.h"
#include "CANIO_componentSpecific.h"
#include "drv_outputAD.h"
// #include "MessageUnpack_generated.h"
// #include "NetworkDefines_generated.h"
// #include "FeatureDefines_generated.h"
// #include "Module.h"
#include "SystemConfig.h"
#include "CANTypes_generated.h"
// #include "ModuleDesc.h"
// #include "HW_can.h"
// #include "HW.h"
// #include "stdint.h"
#include "CAN/CanTypes.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/
static drv_timer_S ledTimer;
static bool tssiRedState = false;


static struct
{
    IMD_State_E state;
} tssi_data;

/******************************************************************************
 *                         P U B L I C  F U N C T I O N S
 ******************************************************************************/

    IMD_State_E imdFault_getState(void)
{
    return tssi_data.state;
}

CAN_digitalStatus_E BMS_getStatusMemCAN(void)
{
    return (tssi_data.state == tssi_GREEN) ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF;
}

static void TSSI_blinkRED_2Hz(void)
{
    //  static bool tssiRedState = false;

    drv_timer_start(&ledTimer, 500U);
    
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_TSSI_R_EN,
    tssiRedState ? DRV_IO_ACTIVE : DRV_IO_INACTIVE);
    
    switch (drv_timer_getState(&ledTimer))
    {
        case DRV_TIMER_EXPIRED:
        {
            tssiRedState = !tssiRedState;
            drv_timer_start(&ledTimer, 500U); //2Hz
            break;
        }
        case DRV_TIMER_RUNNING:
        {
            break;
        }
        case DRV_TIMER_STOPPED:
        {
            //stop the function, assuming stopped is something manual like a reset
            drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_TSSI_R_EN, DRV_IO_INACTIVE);
            tssiRedState = false; // reset the state
            break;
        }
    }
}

/******************************************************************************
 *                         P R I V A T E  F U N C T I O N S
 ******************************************************************************/

void tssi_init(void)
{
    memset(&tssi_data.state, 0, sizeof(tssi_data.state));
    drv_timer_init(&ledTimer);
    drv_timer_start(&ledTimer, 500U);
}

void tssi_reset(void)
{
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_TSSI_R_EN, DRV_IO_INACTIVE);
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_TSSI_G_EN, DRV_IO_INACTIVE); 
}

/******************************************************************************
 *                             P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void tssi_periodic_10Hz(void)
{
    // static bool tssiRedState = false;

    float32_t imdStatus = 0U;
    float32_t bmsStatus = 0U;

    bool imdValid = (CANRX_get_signal(VEH, BMSB_imdStatusMem, &imdStatus) == CANRX_MESSAGE_VALID);
    bool bmsValid = (CANRX_get_signal(VEH, BMSB_bmsStatusMem, &bmsStatus) == CANRX_MESSAGE_VALID);

    if (VEHICLESTATE_ON_GLV)
    {
        if (imdValid && bmsValid && (imdStatus > 0 || bmsStatus > 0))
        {
            tssi_data.state = tssi_RED;
        }
        
        else if (imdValid && bmsValid)
        {
            tssi_data.state = tssi_GREEN;
        }
        
        else
        {
            tssi_data.state = imdFault_INIT;
        }


        if (tssi_data.state == tssi_RED)
        {
            TSSI_blinkRED_2Hz();
            drv_outputAD_setDigitalActiveState(
            DRV_OUTPUTAD_DIGITAL_TSSI_G_EN, DRV_IO_INACTIVE);
        }
        
        else if (tssi_data.state == tssi_GREEN)
        {
            
            drv_timer_stop(&ledTimer);
            tssiRedState = false;
            drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_TSSI_R_EN, DRV_IO_INACTIVE);
            drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_TSSI_G_EN, DRV_IO_ACTIVE);
        }
        else
        {
            drv_timer_stop(&ledTimer);
            tssiRedState = false;
            drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_TSSI_R_EN, DRV_IO_INACTIVE);
            drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_TSSI_G_EN, DRV_IO_INACTIVE);
        }
    }
    else
    {
        drv_timer_stop(&ledTimer);
        tssiRedState = false;
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_TSSI_R_EN, DRV_IO_INACTIVE);
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_TSSI_G_EN, DRV_IO_INACTIVE);
    }
}

const ModuleDesc_S tssiLights_desc = {
    .moduleInit = &tssi_init,
    .periodic10Hz_CLK = &tssi_periodic_10Hz,
};