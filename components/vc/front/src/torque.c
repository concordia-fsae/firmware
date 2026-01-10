/**
 * @file torque.c
 * @brief Torque manager source code for vehicle control
 * @note Units for torque are in Nm
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "torque.h"
#include "apps.h"
#include "bppc.h"
#include "Module.h"
#include "app_vehicleSpeed.h"
#include "ModuleDesc.h"
#include "string.h"
#include "lib_utility.h"
#include "app_vehicleState.h"
#include "MessageUnpack_generated.h"
#include "drv_timer.h"
#include "lib_rateLimit.h"
#include "lib_pid.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define DEFAULT_BOOT_TORQUE 110.0f
#define DEFAULT_TORQUE_PITS 25.0f
#define DEFAULT_TORQUE_LIMIT_REVERSE 20.0f

#define ABSOLUTE_MAX_TORQUE 150.0f
#define ABSOLUTE_MIN_TORQUE 0.0f
#define MIN_TORQUE_RANGE    90.0f
#define MAX_TORQUE_NM_PER_S 500
#define MAX_LAUNCH_NM_PER_S 1000
#define PRELOAD_NM_PER_S    100

#define TORQUE_CHANGE_DELAY 250

#define LC_PRELOAD_TORQUE_INIT 30
#define LC_PRELOAD_TORQUE_MIN 10
#define LC_PRELOAD_TORQUE_MAX 75
#define LC_SETTLING_MS 500
#define LC_REJECTED_MS 2000
#define LC_BRAKE_THRESHOLD 0.30
#define LC_BRAKE_THRESHOLD_LAUNCH 0.05
#define LC_THROTTLE_THRESHOLD 0.50
#define LC_THROTTLE_START_CUTOFF 0.10

#define TC_MAX 0.7f
#define TC_MIN 0.0f
#define TC_TARGET_SLIP 0.10f
#define TC_KP 0.325f
#define TC_KI 0.44f
#define TC_ILIM 0.55f
#define TC_VEHICLESPEED_THRESHOLD_MPS VEHICLE_STOPPED_THRESHOLD

#define PEDAL_SLEEP_THRESHOLD 0.02f
#define SLEEP_TIMEOUT_MS 15*60000

#define PEDAL_APPLIED_THRESHOLD 0.10f
#define VEHICLE_STOPPED_THRESHOLD 0.5

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

static struct
{
    torque_state_E         state;
    torque_gear_E          gear;
    torque_raceMode_E      race_mode;
    float32_t              torque;
    float32_t              torquePreload;
    float32_t              torqueDriverInput;
    float32_t              torque_request_max;
    lib_rateLimit_linear_S torqueRateLimit;
    lib_rateLimit_linear_S launchRateLimit;
    lib_rateLimit_linear_S preloadRateLimit;

    bool gear_change_active;
    bool torque_control_request_active;
    bool race_mode_change_active;

    drv_timer_S torque_change_timer;
    drv_timer_S launch_control_timer;
    drv_timer_S preloadChangeTimer;

    torque_launchControlState_E launchControlState;
    torque_tractionControlState_E tractionControlState;

    uint32_t  lastTimeampMS;
    float32_t slipRear;
    float32_t torqueCorrection;
    float32_t torqueReduction;
    lib_pid_S tractionControlPID;
} torque_data;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static bool evaluate_gear_change(float32_t accelerator_position, float32_t brake_position)
{
    bool ret = false;
    CAN_digitalStatus_E gear_change_request = CAN_DIGITALSTATUS_SNA;
    const bool gear_change_was_requested = torque_data.gear_change_active;
    torque_data.gear_change_active = (CANRX_get_signal(VEH, SWS_requestReverse, &gear_change_request) != CANRX_MESSAGE_SNA) &&
                                     (gear_change_request == CAN_DIGITALSTATUS_ON);
    const bool gear_change_rising = !gear_change_was_requested && torque_data.gear_change_active;
    const float32_t vehicleSpeed = app_vehicleSpeed_getVehicleSpeed();
    const bool ok_to_change = (accelerator_position < PEDAL_APPLIED_THRESHOLD) &&
                              (brake_position > PEDAL_APPLIED_THRESHOLD) &&
                              (vehicleSpeed < VEHICLE_STOPPED_THRESHOLD);

    if (gear_change_rising)
    {
#if FEATURE_IS_ENABLED(FEATURE_REVERSE)
        if (ok_to_change)
        {
            ret = true;
            torque_data.gear = torque_data.gear == GEAR_F ? GEAR_R : GEAR_F;
        }
        else
#endif
        {
            app_faultManager_setFaultState(FM_FAULT_VCFRONT_GEARCHANGEREJECTED, true);
        }
    }
    else
    {
        app_faultManager_setFaultState(FM_FAULT_VCFRONT_GEARCHANGEREJECTED, false);
    }

    return ret;
}

static bool evaluate_mode_change(float32_t brake_position)
{
    bool ret = false;
    CAN_digitalStatus_E race_mode_change_requested = CAN_DIGITALSTATUS_SNA;
    const bool race_mode_change_was_requested = torque_data.race_mode_change_active;
    torque_data.race_mode_change_active = (CANRX_get_signal(VEH, SWS_requestRaceMode, &race_mode_change_requested) != CANRX_MESSAGE_SNA) &&
                                          (race_mode_change_requested == CAN_DIGITALSTATUS_ON);
    const bool race_mode_change_rising = !race_mode_change_was_requested && torque_data.race_mode_change_active;

    const float32_t vehicleSpeed = app_vehicleSpeed_getVehicleSpeed();

    const bool brakePedalPressed = brake_position >= PEDAL_APPLIED_THRESHOLD;
    const bool vehicleStationary = vehicleSpeed < VEHICLE_STOPPED_THRESHOLD;

    if (race_mode_change_rising && brakePedalPressed && vehicleStationary)
    {
        ret = true;
        torque_data.race_mode = torque_data.race_mode == RACEMODE_ENABLED ? RACEMODE_PIT : RACEMODE_ENABLED;
    }

    return ret;
}

static float32_t evaluate_torque_max(void)
{
    float32_t torque_request_max = torque_data.torque_request_max;
    CAN_digitalStatus_E torque_change_request = CAN_DIGITALSTATUS_SNA;
    const bool torque_inc_active = (CANRX_get_signal(VEH, SWS_requestTorqueInc, &torque_change_request) != CANRX_MESSAGE_SNA) &&
                                   (torque_change_request == CAN_DIGITALSTATUS_ON);
    const bool torque_dec_active = (CANRX_get_signal(VEH, SWS_requestTorqueDec, &torque_change_request) != CANRX_MESSAGE_SNA) &&
                                   (torque_change_request == CAN_DIGITALSTATUS_ON);

    if (torque_inc_active ^ torque_dec_active)
    {
        const drv_timer_state_E timer_state = drv_timer_getState(&torque_data.torque_change_timer);
        if (timer_state == DRV_TIMER_STOPPED)
        {
            drv_timer_start(&torque_data.torque_change_timer, TORQUE_CHANGE_DELAY);
            torque_request_max = torque_inc_active ? torque_request_max + 1 : torque_request_max - 1;
            torque_request_max = SATURATE(MIN_TORQUE_RANGE, torque_request_max, ABSOLUTE_MAX_TORQUE);
        }
        else if (timer_state == DRV_TIMER_EXPIRED)
        {
            drv_timer_stop(&torque_data.torque_change_timer);
        }

        torque_data.torque_request_max = torque_request_max;
    }
    else
    {
        drv_timer_stop(&torque_data.torque_change_timer);
    }

    return torque_request_max;
}

static void evaluate_launch_control(float32_t accelerator_position, float32_t brake_position)
{
    bool launchRejected = false;

#if FEATURE_IS_ENABLED(FEATURE_LAUNCH_CONTROL)
    switch (torque_data.launchControlState)
    {
        case LC_STATE_REJECTED:
            launchRejected = true;
            __attribute__((fallthrough));
        case LC_STATE_INACTIVE:
            {
                CAN_digitalStatus_E launch_control_requested = CAN_DIGITALSTATUS_SNA;
                const bool launch_control_request_was_active = torque_data.torque_control_request_active;
                torque_data.torque_control_request_active = (CANRX_get_signal(VEH, SWS_requestLaunchControl, &launch_control_requested) != CANRX_MESSAGE_SNA) &&
                                                            (launch_control_requested == CAN_DIGITALSTATUS_ON);
                const bool launch_control_request_rising = !launch_control_request_was_active && torque_data.torque_control_request_active;

                if (drv_timer_getState(&torque_data.launch_control_timer) == DRV_TIMER_EXPIRED)
                {
                    torque_data.launchControlState = LC_STATE_INACTIVE;
                    drv_timer_stop(&torque_data.launch_control_timer);
                }
                else if (launch_control_request_rising)
                {
                    if ((brake_position > LC_BRAKE_THRESHOLD) &&
                        (accelerator_position < LC_THROTTLE_START_CUTOFF))
                    {
                        torque_data.launchControlState = LC_STATE_HOLDING;
                        drv_timer_stop(&torque_data.launch_control_timer);
                    }
                    else
                    {
                        torque_data.launchControlState = LC_STATE_REJECTED;
                        drv_timer_start(&torque_data.launch_control_timer, LC_REJECTED_MS);
                    }
                }
            }
            break;
        case LC_STATE_HOLDING:
            if (brake_position < LC_BRAKE_THRESHOLD)
            {
                torque_data.launchControlState = LC_STATE_INACTIVE;
            }
            else if (accelerator_position > LC_THROTTLE_THRESHOLD)
            {
                drv_timer_start(&torque_data.launch_control_timer, LC_SETTLING_MS);
                torque_data.launchControlState = LC_STATE_SETTLING;
            }
            break;
        case LC_STATE_SETTLING:
            if (drv_timer_getState(&torque_data.launch_control_timer) == DRV_TIMER_EXPIRED)
            {
                torque_data.launchControlState = LC_STATE_PRELOAD;
                drv_timer_stop(&torque_data.launch_control_timer);
            }
            else if (accelerator_position < LC_THROTTLE_THRESHOLD)
            {
                torque_data.launchControlState = LC_STATE_HOLDING;
            }
            else if (brake_position < LC_BRAKE_THRESHOLD)
            {
                torque_data.launchControlState = LC_STATE_INACTIVE;
            }
            break;
        case LC_STATE_PRELOAD:
            if (accelerator_position < LC_THROTTLE_THRESHOLD)
            {
                torque_data.launchControlState = LC_STATE_INACTIVE;
            }
            else if (brake_position < LC_BRAKE_THRESHOLD_LAUNCH)
            {
                torque_data.launchControlState = LC_STATE_LAUNCH;
            }
            break;
        case LC_STATE_LAUNCH:
            if ((accelerator_position < LC_THROTTLE_THRESHOLD) ||
                (brake_position > LC_BRAKE_THRESHOLD))
            {
                torque_data.launchControlState = LC_STATE_INACTIVE;
            }
            break;
        default:
            break;
    }
#else
    UNUSED(accelerator_position);
    UNUSED(brake_position);
#endif

    app_faultManager_setFaultState(FM_FAULT_VCFRONT_LAUNCHREJECTED, launchRejected);
}

static float32_t calc_traction_control_reduction(float32_t target_slip, float32_t actual_slip, float32_t dt)
{
    lib_pi_typeb_calc(&torque_data.tractionControlPID, target_slip, actual_slip, dt);
    lib_pid_util_ilim(&torque_data.tractionControlPID, 0.0f, TC_ILIM);
    lib_pid_typeb_sum(&torque_data.tractionControlPID, TC_MIN, TC_MAX);

    return torque_data.tractionControlPID.y;
}

static float32_t evaluate_traction_control(void)
{
    const uint32_t timestamp = HW_TIM_getTimeMS();
    const float32_t dt = ((float32_t)(timestamp - torque_data.lastTimeampMS)) / 1000.0f;
    torque_data.lastTimeampMS = timestamp;

    const float32_t vehicleSpeed = app_vehicleSpeed_getVehicleSpeed();
    const float32_t slip = app_vehicleSpeed_getAxleSlip(AXLE_REAR);
    float32_t multiplier = 0.0f;

    torque_tractionControlState_E nextState = TC_STATE_ERROR;
    if ((vehicleSpeed > TC_VEHICLESPEED_THRESHOLD_MPS) &&
        (torque_data.gear == GEAR_F) &&
        (torque_data.race_mode == RACEMODE_ENABLED))
    {
#if FEATURE_IS_ENABLED(FEATURE_TRACTION_CONTROL)
        CAN_digitalStatus_E traction_control_requested = CAN_DIGITALSTATUS_SNA;
        bool requested = (CANRX_get_signal(VEH, SWS_requestTractionControl, &traction_control_requested) != CANRX_MESSAGE_SNA) &&
                            (traction_control_requested == CAN_DIGITALSTATUS_ON);
        if (requested)
        {
            nextState = TC_STATE_ACTIVE;
        }
        else
#endif
        {
            nextState = TC_STATE_INACTIVE;
        }
    }
    else
    {
        nextState = TC_STATE_LOCKOUT;
    }

    torque_data.tractionControlState = nextState;

    if (torque_data.tractionControlState == TC_STATE_ACTIVE)
    {
        multiplier = calc_traction_control_reduction(TC_TARGET_SLIP, slip, dt);
    }
    else
    {
        lib_pid_init(&torque_data.tractionControlPID, 0.0f, 0.0f, TC_KP, TC_KI, 0.0f);
    }

    torque_data.slipRear = slip;
    return multiplier;
}

static void evaluate_sleepable(float32_t accelerator_position, float32_t brake_position)
{
    if ((accelerator_position > PEDAL_SLEEP_THRESHOLD) || (brake_position > PEDAL_SLEEP_THRESHOLD))
    {
        app_vehicleState_delaySleep(SLEEP_TIMEOUT_MS);
    }
}

static void evaluate_preload_torque(void)
{
    float32_t torque_request = torque_data.torquePreload;
    CAN_digitalStatus_E torque_change_request = CAN_DIGITALSTATUS_SNA;
    const bool torque_inc_active = (CANRX_get_signal(VEH, SWS_requestPreloadTorqueInc, &torque_change_request) != CANRX_MESSAGE_SNA) &&
                                   (torque_change_request == CAN_DIGITALSTATUS_ON);
    const bool torque_dec_active = (CANRX_get_signal(VEH, SWS_requestPreloadTorqueDec, &torque_change_request) != CANRX_MESSAGE_SNA) &&
                                   (torque_change_request == CAN_DIGITALSTATUS_ON);

    if (torque_inc_active ^ torque_dec_active)
    {
        const drv_timer_state_E timer_state = drv_timer_getState(&torque_data.preloadChangeTimer);
        if (timer_state == DRV_TIMER_STOPPED)
        {
            drv_timer_start(&torque_data.preloadChangeTimer, TORQUE_CHANGE_DELAY);
            torque_request = torque_inc_active ? torque_request + 1 : torque_request - 1;
        }
        else if (timer_state == DRV_TIMER_EXPIRED)
        {
            drv_timer_stop(&torque_data.preloadChangeTimer);
        }

        torque_data.torquePreload = SATURATE(LC_PRELOAD_TORQUE_MIN, torque_request, LC_PRELOAD_TORQUE_MAX);
    }
    else
    {
        drv_timer_stop(&torque_data.preloadChangeTimer);
    }
}

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * @brief Get the current torque request
 * @return Current torque request in Nm
 */
float32_t torque_getTorqueRequest(void)
{
    return torque_data.torque;
}

/**
 * @brief Get the max torque request
 * @return Max torque request in Nm
 */
float32_t torque_getTorqueRequestMax(void)
{
    return torque_data.torque_request_max;
}

/**
 * @brief Get the raw driver input torque
 * @return Raw driver input torque
 */
float32_t torque_getTorqueDriverInput(void)
{
    return torque_data.torqueDriverInput;
}

/**
 * @brief Get the max torque request
 * @return Max torque request in Nm
 */
float32_t torque_getTorqueRequestCorrection(void)
{
    return torque_data.torqueCorrection;
}

/**
 * @brief Get current torque manager state
 * @return CAN state of the torque manager
 */
torque_state_E torque_getState(void)
{
    return torque_data.state;
}

float32_t torque_getSlipRaw(void)
{
    return torque_data.slipRear;
}

float32_t torque_getSlipErrorP(void)
{
    return torque_data.tractionControlPID.p_term;
}

float32_t torque_getSlipErrorI(void)
{
    return torque_data.tractionControlPID.i_term;
}

float32_t torque_getSlipErrorD(void)
{
    return torque_data.tractionControlPID.d_term;
}

float32_t torque_getTorqueReduction(void)
{
    return torque_data.torqueReduction;
}

float32_t torque_getPreloadTorque(void)
{
    return torque_data.torquePreload;
}

/**
 * @brief Get current gear
 * @return State of the torque manager
 */
torque_gear_E torque_getGear(void)
{
    return torque_data.gear;
}

/**
 * @brief Translate gear state to CAN
 * @return CAN state of the torque manager gear
 */
CAN_gear_E torque_getGearCAN(void)
{
    CAN_gear_E ret = CAN_GEAR_SNA;

    switch (torque_data.gear)
    {
        case GEAR_F:
            ret = CAN_GEAR_FORWARD;
            break;
        case GEAR_R:
            ret = CAN_GEAR_REVERSE;
            break;
        default:
            break;
    }

    return ret;
}

/**
 * @brief Get current launch control state
 * @return Launch Control State
 */
torque_launchControlState_E torque_getLaunchControlState(void)
{
    return torque_data.launchControlState;
}

bool torque_isLaunching(void)
{
    const bool launching = (torque_data.launchControlState == LC_STATE_HOLDING) ||
                           (torque_data.launchControlState == LC_STATE_SETTLING) ||
                           (torque_data.launchControlState == LC_STATE_PRELOAD) ||
                           (torque_data.launchControlState == LC_STATE_LAUNCH);
    return launching;
}

/**
 * @brief Translate launch control state to CAN
 * @return CAN state of the launch control state
 */
CAN_launchControlState_E torque_getLaunchControlStateCAN(void)
{
    CAN_launchControlState_E ret = CAN_LAUNCHCONTROLSTATE_SNA;

    switch (torque_data.launchControlState)
    {
        case LC_STATE_INACTIVE:
            ret = CAN_LAUNCHCONTROLSTATE_INACTIVE;
            break;
        case LC_STATE_HOLDING:
            ret = CAN_LAUNCHCONTROLSTATE_HOLDING;
            break;
        case LC_STATE_SETTLING:
            ret = CAN_LAUNCHCONTROLSTATE_SETTLING;
            break;
        case LC_STATE_PRELOAD:
            ret = CAN_LAUNCHCONTROLSTATE_PRELOAD;
            break;
        case LC_STATE_LAUNCH:
            ret = CAN_LAUNCHCONTROLSTATE_LAUNCH;
            break;
        case LC_STATE_REJECTED:
            ret = CAN_LAUNCHCONTROLSTATE_REJECTED;
            break;
        case LC_STATE_ERROR:
            ret = CAN_LAUNCHCONTROLSTATE_ERROR;
            break;
        default:
            break;
    }

    return ret;
}

/**
 * @brief Translate torque state to CAN
 * @return CAN state of the torque manager
 */
CAN_torqueManagerState_E torque_getStateCAN(void)
{
    CAN_torqueManagerState_E ret = CAN_TORQUEMANAGERSTATE_SNA;

    switch (torque_data.state)
    {
        case TORQUE_INACTIVE:
            ret = CAN_TORQUEMANAGERSTATE_INACTIVE;
            break;
        case TORQUE_ACTIVE:
            ret = CAN_TORQUEMANAGERSTATE_ACTIVE;
            break;
        default:
            break;
    }

    return ret;
}

/**
 * @brief Get current race mode
 * @return State of the torque manager race mode
 */
torque_raceMode_E torque_getRaceMode(void)
{
 return torque_data.race_mode;
}

/**
 * @brief Translate race mode to CAN
 * @return CAN state of the torque manager race mode
 */
CAN_raceMode_E torque_getRaceModeCAN(void)
{
    CAN_raceMode_E ret = CAN_RACEMODE_PIT;

    switch (torque_data.race_mode)
    {
        case RACEMODE_ENABLED:
            ret = CAN_RACEMODE_RACE;
            break;
        default:
            break;
    }

    return ret;
}

torque_tractionControlState_E torque_getTractionControlState(void)
{
    return torque_data.tractionControlState;
}

CAN_tractionControlState_E torque_getTractionControlStateCAN(void)
{
    CAN_tractionControlState_E ret = CAN_TRACTIONCONTROLSTATE_SNA;

    switch (torque_data.tractionControlState)
    {
        case TC_STATE_INACTIVE:
            ret = CAN_TRACTIONCONTROLSTATE_INACTIVE;
            break;
        case TC_STATE_ACTIVE:
            ret = CAN_TRACTIONCONTROLSTATE_ACTIVE;
            break;
        case TC_STATE_FAULT_SENSOR:
            ret = CAN_TRACTIONCONTROLSTATE_FAULT_SENSOR;
            break;
        case TC_STATE_ERROR:
            ret = CAN_TRACTIONCONTROLSTATE_ERROR;
            break;
        case TC_STATE_LOCKOUT:
            ret = CAN_TRACTIONCONTROLSTATE_LOCKOUT;
            break;
        default:
            break;
    }

    return ret;
}

static void torque_init(void)
{
    memset(&torque_data, 0x00U, sizeof(torque_data));

    drv_timer_init(&torque_data.torque_change_timer);
    drv_timer_init(&torque_data.launch_control_timer);
    drv_timer_init(&torque_data.preloadChangeTimer);

    torque_data.state = TORQUE_INACTIVE;
    torque_data.torque_request_max = DEFAULT_BOOT_TORQUE;
    torque_data.gear = GEAR_F;
    torque_data.launchControlState   = LC_STATE_INACTIVE;
    torque_data.tractionControlState = TC_STATE_INACTIVE;

    torque_data.torqueRateLimit.y_n          = 0.0f;
    torque_data.torqueRateLimit.maxStepDelta = MAX_TORQUE_NM_PER_S / 100;
    torque_data.launchRateLimit.y_n          = 0.0f;
    torque_data.launchRateLimit.maxStepDelta = MAX_LAUNCH_NM_PER_S / 100;
    torque_data.preloadRateLimit.y_n          = 0.0f;
    torque_data.preloadRateLimit.maxStepDelta = PRELOAD_NM_PER_S / 100;

    torque_data.torquePreload = LC_PRELOAD_TORQUE_INIT;

    lib_pid_init(&torque_data.tractionControlPID, 0.0f, 0.0f, TC_KP, TC_KI, 0.0f);
}

static void torque_periodic_100Hz(void)
{
    const float32_t accelerator_position = apps_getPedalPosition();
    const float32_t brake_position = bppc_getPedalPosition();
    const bppc_state_E bppc_ok = bppc_getState() == BPPC_OK;
    torque_data.state = app_vehicleState_getState() == VEHICLESTATE_TS_RUN ? TORQUE_ACTIVE : TORQUE_INACTIVE;
    evaluate_sleepable(accelerator_position, brake_position);

    const bool gear_change = evaluate_gear_change(accelerator_position, brake_position);
    const bool mode_change = evaluate_mode_change(brake_position);
    evaluate_launch_control(accelerator_position, brake_position);
    torque_data.torqueReduction = evaluate_traction_control();

    float32_t torque_request_max = evaluate_torque_max();
    evaluate_preload_torque();

    if (torque_data.race_mode != RACEMODE_ENABLED)
    {
        torque_request_max = DEFAULT_TORQUE_PITS;
    }
    if (torque_data.gear != GEAR_F)
    {
        torque_request_max = DEFAULT_TORQUE_LIMIT_REVERSE;
    }

    if (gear_change || mode_change || !bppc_ok)
    {
        torque_data.torqueRateLimit.y_n = 0.0f;
    }

    float32_t torque = (bppc_ok) ? accelerator_position * torque_request_max : 0.0f;
    torque_data.torqueDriverInput = torque;
    torque_data.torqueCorrection = torque_data.torqueReduction * torque;

    if ((torque_data.launchControlState == LC_STATE_HOLDING) ||
        (torque_data.launchControlState == LC_STATE_SETTLING))
    {
        torque = 0.0f;
        torque_data.torqueRateLimit.y_n = 0.0f;
        torque_data.launchRateLimit.y_n = 0.0f;
        torque_data.preloadRateLimit.y_n = 0.0f;
    }
    else if (torque_data.launchControlState == LC_STATE_PRELOAD)
    {
        torque = torque_data.torquePreload;
        torque = lib_rateLimit_linear_update(&torque_data.preloadRateLimit, torque);
        torque_data.launchRateLimit.y_n = torque;
    }
    else if (torque_data.launchControlState == LC_STATE_LAUNCH)
    {
        torque = lib_rateLimit_linear_update(&torque_data.launchRateLimit, torque);
        torque_data.torqueRateLimit.y_n = torque;
    }
    else
    {
        torque -= torque_data.torqueCorrection;
        torque = lib_rateLimit_linear_update(&torque_data.torqueRateLimit, torque);
    }

    torque_data.torque = SATURATE(ABSOLUTE_MIN_TORQUE, torque, ABSOLUTE_MAX_TORQUE);
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S torque_desc = {
    .moduleInit = &torque_init,
    .periodic100Hz_CLK = &torque_periodic_100Hz,
};
