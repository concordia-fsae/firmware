/**
 * @file cooling.c
 * @brief  Source code for Cooling Application
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Module.h"
#include "drv_vn9008.h"
#include "drv_outputAD.h"
#include "app_vehicleState.h"
#include "HW_tim.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Cooling Module Init function
 */
static void cooling_init()
{
#if FEATURE_IS_ENABLED(FEATURE_PUMP_FULL_BEANS)
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_PWM1, DRV_IO_ACTIVE);
#endif
}

/**
 * @brief  Cooling Module 1Hz periodic function
 */
static void cooling10Hz_PRD(void)
{
    CAN_digitalStatus_E tmp = CAN_DIGITALSTATUS_SNA;
    const bool testPump = (CANRX_get_signal(VEH, SWS_requestTestPump, &tmp) == CANRX_MESSAGE_VALID) &&
                          (tmp == CAN_DIGITALSTATUS_ON);
    const bool testFan = (CANRX_get_signal(VEH, SWS_requestTestFan, &tmp) == CANRX_MESSAGE_VALID) &&
                         (tmp == CAN_DIGITALSTATUS_ON);
    const bool isHV  = app_vehicleState_getState() == VEHICLESTATE_ON_HV;
    const bool isRun = app_vehicleState_getState() == VEHICLESTATE_TS_RUN;

    const bool faultPump = (drv_vn9008_getState(DRV_VN9008_CHANNEL_PUMP) == DRV_HSD_STATE_OVERCURRENT) ||
                           (drv_vn9008_getState(DRV_VN9008_CHANNEL_PUMP) == DRV_HSD_STATE_OVERTEMP);
    const bool faultFan = (drv_vn9008_getState(DRV_VN9008_CHANNEL_FAN) == DRV_HSD_STATE_OVERCURRENT) ||
                          (drv_vn9008_getState(DRV_VN9008_CHANNEL_FAN) == DRV_HSD_STATE_OVERTEMP);

    drv_vn9008_setEnabled(DRV_VN9008_CHANNEL_PUMP, (isHV || isRun || testPump) && !faultPump);
    drv_vn9008_setEnabled(DRV_VN9008_CHANNEL_FAN, (isRun || testFan) && !faultFan);

    if (isHV || isRun || testPump)
    {
#if FEATURE_IS_DISABLED(FEATURE_PUMP_FULL_BEANS)
        HW_TIM_setDuty(HW_TIM_PORT_PUMP, HW_TIM_CHANNEL_1, 0.75f);
#endif
    }
    else
    {
#if FEATURE_IS_DISABLED(FEATURE_PUMP_FULL_BEANS)
        HW_TIM_setDuty(HW_TIM_PORT_PUMP, HW_TIM_CHANNEL_1, 0.00f);
#endif
    }
}

/**
 * @brief  Cooling Module descriptor
 */
const ModuleDesc_S cooling_desc = {
    .moduleInit       = &cooling_init,
    .periodic10Hz_CLK = &cooling10Hz_PRD,
};
