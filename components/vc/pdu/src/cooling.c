/**
 * @file cooling.c
 * @brief  Source code for Cooling Application
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "app_vehicleState.h"
#include "drv_outputAD.h"
#include "drv_timer.h"
#include "drv_vn9008.h"
#include "HW_tim.h"
#include "Module.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define START_TIMER_MS                 500U
#define START_DUTY                     1.0f
#define FAN_ON_DUTY                    0.8f
#define PUMP_ON_DUTY                   1.0f
#define TEST_DUTY                      1.0f
#define FAN_ON_DUTY_HOT                1.0f

#define COOLING_LATCH_START_THRESH     50.0f
#define COOLING_LATCH_STOP_THRESH      45.0f
#define HOT_DRIVETRAIN_START_THRESH    60.0f
#define HOT_DRIVETRAIN_STOP_THRESH     55.0f

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    drv_timer_S enableTimerFan;
    drv_timer_S enableTimerPump;
    bool        drivetrainCoolingLatched;
    bool        drivetrainHot;
} cooling;

static struct
{
    bool isTestPump;
    bool isTestFan;
    bool wasRequestPump;
    bool wasRequestFan;
} test;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void setDuty(drv_vn9008_E channel, float32_t duty, drv_timer_S* enableTimer, bool recover)
{
    const drv_timer_state_E timerState        = drv_timer_getState(enableTimer);
    const bool              startTimerExpired = timerState == DRV_TIMER_EXPIRED;
    const bool              startTimerRunning = timerState == DRV_TIMER_RUNNING;

    if (recover)
    {
        drv_timer_stop(enableTimer);
        duty = 0.0f;
    }
    if ((duty > 0.0f) && !startTimerExpired)
    {
        if (!startTimerRunning)
        {
            drv_timer_start(enableTimer, START_TIMER_MS);
        }

        duty = (duty > START_DUTY) ? duty : START_DUTY;
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
    test.wasRequestFan    = false;
    test.wasRequestPump   = false;
    test.isTestPump       = false;
    test.isTestFan        = false;
    cooling.drivetrainHot = false;
}

/**
 * @brief  Cooling Module 1Hz periodic function
 */
static void cooling10Hz_PRD(void)
{
    float32_t motorTemp = 0.0f;

    (void)CANRX_get_signal(VEH, PM100DX_motorTemp, &motorTemp);

    if (cooling.drivetrainCoolingLatched)
    {
        cooling.drivetrainCoolingLatched = motorTemp > COOLING_LATCH_STOP_THRESH;
    }
    else
    {
        cooling.drivetrainCoolingLatched = motorTemp > COOLING_LATCH_START_THRESH;
    }

    CAN_digitalStatus_E requestChangePump = CAN_DIGITALSTATUS_SNA;
    CAN_digitalStatus_E requestChangeFan  = CAN_DIGITALSTATUS_SNA;
    const bool          requestedPump     = (CANRX_get_signal(VEH, SWS_requestTestPump, &requestChangePump) == CANRX_MESSAGE_VALID) && (requestChangePump == CAN_DIGITALSTATUS_ON);
    const bool          requestedFan      = (CANRX_get_signal(VEH, SWS_requestTestFan, &requestChangeFan) == CANRX_MESSAGE_VALID) && (requestChangeFan == CAN_DIGITALSTATUS_ON);
    const bool          isHV              = app_vehicleState_getState() == VEHICLESTATE_ON_HV;
    const bool          isRun             = app_vehicleState_getState() == VEHICLESTATE_TS_RUN;

    const bool          faultPump         = (drv_vn9008_getState(DRV_VN9008_CHANNEL_PUMP) == DRV_HSD_STATE_OVERCURRENT) ||
                                            (drv_vn9008_getState(DRV_VN9008_CHANNEL_PUMP) == DRV_HSD_STATE_OVERTEMP);
    const bool          faultFan          = (drv_vn9008_getState(DRV_VN9008_CHANNEL_FAN) == DRV_HSD_STATE_OVERCURRENT) ||
                                            (drv_vn9008_getState(DRV_VN9008_CHANNEL_FAN) == DRV_HSD_STATE_OVERTEMP);

    test.isTestPump    ^= (!test.wasRequestPump && requestedPump);
    test.isTestFan     ^= (!test.wasRequestFan && requestedFan);
    test.wasRequestPump = requestedPump;
    test.wasRequestFan  = requestedFan;
    test.isTestFan     &= !faultFan;
    test.isTestPump    &= !faultPump;

    if (motorTemp > HOT_DRIVETRAIN_START_THRESH)
    {
        cooling.drivetrainHot = true;
    }
    if (motorTemp < HOT_DRIVETRAIN_STOP_THRESH)
    {
        cooling.drivetrainHot = false;
    }
    float32_t  fanDutyCycle = cooling.drivetrainHot ? FAN_ON_DUTY_HOT : FAN_ON_DUTY;
    float32_t  dutyFan      = test.isTestFan? TEST_DUTY: fanDutyCycle;
    float32_t  dutyPump     = test.isTestPump ? TEST_DUTY : PUMP_ON_DUTY;
    const bool enablePump   = (isHV || isRun || test.isTestPump || cooling.drivetrainCoolingLatched) && !faultPump;
    const bool enableFan    = (isRun || test.isTestFan || cooling.drivetrainCoolingLatched) && !faultFan;

    setDuty(DRV_VN9008_CHANNEL_PUMP, enablePump ? dutyPump : 0.0f, &cooling.enableTimerPump, faultPump);
    setDuty(DRV_VN9008_CHANNEL_FAN,  enableFan ? dutyFan : 0.0f,   &cooling.enableTimerFan,  faultFan);
}

/**
 * @brief  Cooling Module descriptor
 */
const ModuleDesc_S cooling_desc = {
    .moduleInit       = &cooling_init,
    .periodic10Hz_CLK = &cooling10Hz_PRD,
};
