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

#define START_TIMER_MS 50U
#define START_DUTY     0.75f
#define FAN_ON_DUTY    0.4f
#define PUMP_ON_DUTY   0.6f

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    drv_timer_S enableTimerFan;
    drv_timer_S enableTimerPump;
} cooling;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void setDuty(drv_vn9008_E channel, float32_t duty, drv_timer_S* enableTimer)
{
    const drv_timer_state_E timerState = drv_timer_getState(enableTimer);
    const bool startTimerExpired = timerState == DRV_TIMER_EXPIRED;
    const bool startTimerRunning = timerState == DRV_TIMER_RUNNING;

    if ((duty > 0.0f) && !startTimerExpired)
    {
        if (!startTimerRunning)
        {
            drv_timer_start(enableTimer, START_TIMER_MS);
        }

        duty = duty > START_DUTY ? duty : START_DUTY;
    }
    else if (duty <= 0.0f)
    {
        drv_timer_stop(enableTimer);
    }

    drv_vn9008_setDuty(channel, duty);
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
    const float32_t dutyFan = testFan ? 1.0f : FAN_ON_DUTY;
    const float32_t dutyPump = testPump ? 1.0f : PUMP_ON_DUTY;
    const bool enablePump = (isHV || isRun || testPump) && !faultPump;
    const bool enableFan = (isRun || testFan) && !faultFan;

    setDuty(DRV_VN9008_CHANNEL_PUMP, enablePump ? dutyPump : 0.0f, &cooling.enableTimerPump);
    setDuty(DRV_VN9008_CHANNEL_FAN, enableFan ? dutyFan : 0.0f, &cooling.enableTimerFan);
}

/**
 * @brief  Cooling Module descriptor
 */
const ModuleDesc_S cooling_desc = {
    .moduleInit       = &cooling_init,
    .periodic10Hz_CLK = &cooling10Hz_PRD,
};
