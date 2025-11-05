/**
 * @file tssi.c
 * @brief Module source that manages the tssi
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "tssi.h"
#include "LIB_Types.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "string.h"
#include "MessageUnpack_generated.h"
#include "drv_outputAD.h"
#include "app_vehicleState.h"
#include "drv_timer.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    tssi_state_E state;
    drv_timer_S  timer;
} tssi_data;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void setGreen(void){
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_TSSI_G_EN, DRV_IO_ACTIVE);
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_TSSI_R_EN, DRV_IO_INACTIVE);
}

static void setRed(void){
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_TSSI_G_EN, DRV_IO_INACTIVE);

    if (drv_timer_getState(&tssi_data.timer) == DRV_TIMER_EXPIRED)
    {
        drv_timer_start(&tssi_data.timer, 250U);
        drv_outputAD_toggleDigitalState(DRV_OUTPUTAD_DIGITAL_TSSI_R_EN);
    }
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

tssi_state_E tssi_getState(void)
{
    return tssi_data.state;
}

CAN_tssiState_E tssi_getStateCAN(void)
{
    switch (tssi_data.state)
    {
        case TSSI_ON_RED:
            return CAN_TSSISTATE_ON_RED;
        case TSSI_ON_GREEN:
            return CAN_TSSISTATE_ON_GREEN;
        default:
            return CAN_TSSISTATE_SNA;
    }
}

static void tssi_init(void)
{
    memset(&tssi_data, 0x00U, sizeof(tssi_data));
    tssi_data.state = TSSI_INIT;
    drv_timer_init(&tssi_data.timer);
    setGreen();
}

static void tssi_periodic_10Hz(void)
{
    CAN_digitalStatus_E bmsStatus = CAN_DIGITALSTATUS_SNA;
    CAN_digitalStatus_E imdStatus = CAN_DIGITALSTATUS_SNA;
    CAN_digitalStatus_E reset_sig = CAN_DIGITALSTATUS_SNA;

    const bool reset_valid = (CANRX_get_signal(VEH, BMSB_bmsIMDReset, &reset_sig) == CANRX_MESSAGE_VALID);
    const bool imd_valid = (CANRX_get_signal(VEH, BMSB_imdStatusMem, &imdStatus) == CANRX_MESSAGE_VALID);
    const bool bms_valid = (CANRX_get_signal(VEH, BMSB_bmsStatusMem, &bmsStatus) == CANRX_MESSAGE_VALID);
    const bool bms_uds_valid = CANRX_validate(VEH, UDSCLIENT_bmsbUdsRequest) == CANRX_MESSAGE_VALID;

    if (bms_uds_valid || tssi_data.state == TSSI_INIT)
    {
        setGreen();
        tssi_data.state = TSSI_INIT;
        if (reset_valid && (reset_sig == CAN_DIGITALSTATUS_ON))
        {
            // This will delay the next run of the TSSI by 100ms, allowing enough time for the state of the
            // shutdown circuit to update and be re-transmitted over CAN
            tssi_data.state = TSSI_ON_GREEN;
        }
        return;
    }

    if ((imd_valid == true ) && (bms_valid == true) &&
        (bmsStatus == CAN_DIGITALSTATUS_ON) && (imdStatus == CAN_DIGITALSTATUS_ON))
    {
        drv_timer_stop(&tssi_data.timer);
        tssi_data.state = TSSI_ON_GREEN;
    }
    else if (tssi_data.state == TSSI_ON_GREEN)
    {
        drv_timer_start(&tssi_data.timer, 250);
        tssi_data.state = TSSI_ON_RED;
    }

    switch (tssi_data.state)
    {
        case TSSI_ON_GREEN:
            setGreen();
            break;
        default:
            setRed();
    }
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S tssi_desc = {
    .moduleInit = &tssi_init,
    .periodic10Hz_CLK = &tssi_periodic_10Hz,
};
