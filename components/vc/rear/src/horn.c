/**
 * @file horn.c
 * @brief Module source that manages the horn
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "horn.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "string.h"
#include "drv_outputAD.h"
#include "app_vehicleState.h"
#include "drv_timer.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define HORN_ON_TIME_MS 1500U

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    horn_state_E state;
    drv_timer_S  timer;
} horn_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

horn_state_E horn_getState(void)
{
    return horn_data.state;
}

CAN_digitalStatus_E horn_getStateCAN(void)
{
    return (horn_data.state == HORN_ON) ? CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF;
}

static void horn_init(void)
{
    memset(&horn_data, 0x00U, sizeof(horn_data));

    horn_data.state = HORN_OFF;
    drv_timer_init(&horn_data.timer);
}

static void horn_periodic_10Hz(void)
{
    if (app_vehicleState_getState() == VEHICLESTATE_TS_RUN)
    {
        if (drv_timer_getState(&horn_data.timer) == DRV_TIMER_STOPPED)
        {
            drv_timer_start(&horn_data.timer, HORN_ON_TIME_MS);
            horn_data.state = HORN_ON;
        }
        else if (drv_timer_getState(&horn_data.timer) == DRV_TIMER_EXPIRED)
        {
            horn_data.state = HORN_OFF;
        }
    }
    else
    {
        drv_timer_stop(&horn_data.timer);
        horn_data.state = HORN_OFF;
    }

    switch (horn_data.state)
    {
        case HORN_ON:
            drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_HORN_EN, DRV_IO_ACTIVE);
            break;
        case HORN_OFF:
        default:
            drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_HORN_EN, DRV_IO_INACTIVE);
            break;
    }
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S horn_desc = {
    .moduleInit = &horn_init,
    .periodic10Hz_CLK = &horn_periodic_10Hz,
};
