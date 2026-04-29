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
#include "drv_timer.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define FAN_START_TIMER_MS 50U
#define FAN_START_DUTY     0.75f
#define FAN_ON_DUTY        0.4f
#define PUMP_ON_DUTY       0.8f

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    drv_timer_S enableTimerFan;
} cooling;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void setFanDuty(float32_t duty)
{
    const drv_timer_state_E timerState = drv_timer_getState(&cooling.enableTimerFan);
    const bool startTimerExpired = timerState == DRV_TIMER_EXPIRED;
    const bool startTimerRunning = timerState == DRV_TIMER_RUNNING;

    if ((duty > 0.0f) && !startTimerExpired)
    {
        if (!startTimerRunning)
        {
            drv_timer_start(&cooling.enableTimerFan, FAN_START_TIMER_MS);
        }

        duty = FAN_START_DUTY;
    }
    else if (duty <= 0.0f)
    {
        drv_timer_stop(&cooling.enableTimerFan);
    }

    drv_vn9008_setDuty(DRV_VN9008_CHANNEL_FAN, duty);
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Cooling Module Init function
 */
static void cooling_init()
{
    drv_timer_init(&cooling.enableTimerFan);
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

    drv_vn9008_setDuty(DRV_VN9008_CHANNEL_PUMP, ((isHV || isRun || testPump) && !faultPump) ? PUMP_ON_DUTY : 0.0f);
    setFanDuty(((isRun || testFan) && !faultFan) ? FAN_ON_DUTY : 0.0f);
}

/**
 * @brief  Cooling Module descriptor
 */
const ModuleDesc_S cooling_desc = {
    .moduleInit       = &cooling_init,
    .periodic10Hz_CLK = &cooling10Hz_PRD,
};
