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
#include "ModuleDesc.h"
#include "string.h"
#include "lib_utility.h"
#include "app_vehicleState.h"
#include "MessageUnpack_generated.h"
#include "drv_timer.h"
#include "pid.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define DEFAULT_TORQUE_PITS 25.0f
#define DEFAULT_TORQUE_LIMIT_REVERSE 20.0f

#define ABSOLUTE_MAX_TORQUE 130.0f
#define ABSOLUTE_MIN_TORQUE 0.0f
#define MIN_TORQUE_RANGE 90.0f

#define DEFAULT_BOOT_TORQUE 110.0f

#define TORQUE_CHANGE_DELAY 250

#define LAUNCH_CONTROL_SETTLING_MS 500
#define LAUNCH_CONTROL_REJECTED_MS 5000

#define LAUNCH_CONTROL_BRAKE_THRESHOLD 0.30
#define LAUNCH_CONTROL_BRAKE_THRESHOLD_LAUNCH 0.05
#define LAUNCH_CONTROL_THROTTLE_THRESHOLD 0.50
#define LAUNCH_CONTROL_THROTTLE_START_CUTOFF 0.10

#define TRACTION_CONTROL_TARGET_SLIP 0.10f

#define TRACTION_CONTROL_KP 800 // Starting Number: If we slip 1% extra, reduce 8Nm
#define TRACTION_CONTROL_KI 1000   // Starting Number: If we integrate 1% slip error, reduce 10Nm
#define TRACTION_CONTROL_ILIM (MIN_TORQUE_RANGE / TRACTION_CONTROL_KI)

#define TRACTIONCONTROL_RAXLE_THRESHOLD_RPM 60

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

static struct
{
    torque_state_E    state;
    torque_gear_E     gear;
    torque_raceMode_E race_mode;
    float32_t         torque;
    float32_t         torque_request_max;

    bool gear_change_active;
    bool torque_control_request_active;
    bool race_mode_change_active;

    drv_timer_S torque_change_timer;
    drv_timer_S launch_control_timer;

    torque_launchControl_E launch_control_state;
    torque_tractionControl_E traction_control_state;

    float32_t torque_correction;
    epid_t traction_control_pid;
} torque_data;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void evaluate_gear_change(void)
{
    CAN_digitalStatus_E gear_change_request = CAN_DIGITALSTATUS_SNA;
    const bool gear_change_was_requested = torque_data.gear_change_active;
    torque_data.gear_change_active = (CANRX_get_signal(VEH, SWS_requestReverse, &gear_change_request) != CANRX_MESSAGE_SNA) &&
                                     (gear_change_request == CAN_DIGITALSTATUS_ON);
    const bool gear_change_rising = !gear_change_was_requested && torque_data.gear_change_active;

#if FEATURE_IS_ENABLED(FEATURE_REVERSE)
    if (gear_change_rising)
    {
        torque_data.gear = torque_data.gear == GEAR_F ? GEAR_R : GEAR_F;
    }
#endif
}

static void evaluate_mode_change(void)
{
    CAN_digitalStatus_E race_mode_change_requested = CAN_DIGITALSTATUS_SNA;
    const bool race_mode_change_was_requested = torque_data.race_mode_change_active;
    torque_data.race_mode_change_active = (CANRX_get_signal(VEH, SWS_requestRaceMode, &race_mode_change_requested) != CANRX_MESSAGE_SNA) &&
                                          (race_mode_change_requested == CAN_DIGITALSTATUS_ON);
    const bool race_mode_change_rising = !race_mode_change_was_requested && torque_data.race_mode_change_active;

    if (race_mode_change_rising)
    {
        torque_data.race_mode = torque_data.race_mode == RACEMODE_ENABLED ? RACEMODE_PIT : RACEMODE_ENABLED;
    }
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
#if FEATURE_IS_ENABLED(FEATURE_LAUNCH_CONTROL)
    switch (torque_data.launch_control_state)
    {
        case LAUNCHCONTROL_REJECTED:
        case LAUNCHCONTROL_INACTIVE:
            {
                CAN_digitalStatus_E launch_control_requested = CAN_DIGITALSTATUS_SNA;
                const bool launch_control_request_was_active = torque_data.torque_control_request_active;
                torque_data.torque_control_request_active = (CANRX_get_signal(VEH, SWS_requestLaunchControl, &launch_control_requested) != CANRX_MESSAGE_SNA) &&
                                                            (launch_control_requested == CAN_DIGITALSTATUS_ON);
                const bool launch_control_request_rising = !launch_control_request_was_active && torque_data.torque_control_request_active;

                if (drv_timer_getState(&torque_data.launch_control_timer) == DRV_TIMER_EXPIRED)
                {
                    torque_data.launch_control_state = LAUNCHCONTROL_INACTIVE;
                    drv_timer_stop(&torque_data.launch_control_timer);
                }
                else if (launch_control_request_rising)
                {
                    if ((brake_position > LAUNCH_CONTROL_BRAKE_THRESHOLD) &&
                        (accelerator_position < LAUNCH_CONTROL_THROTTLE_START_CUTOFF))
                    {
                        torque_data.launch_control_state = LAUNCHCONTROL_HOLDING;
                        drv_timer_stop(&torque_data.launch_control_timer);
                    }
                    else
                    {
                        torque_data.launch_control_state = LAUNCHCONTROL_REJECTED;
                        drv_timer_start(&torque_data.launch_control_timer, LAUNCH_CONTROL_REJECTED_MS);
                    }
                }
            }
            break;
        case LAUNCHCONTROL_HOLDING:
            if (brake_position < LAUNCH_CONTROL_BRAKE_THRESHOLD)
            {
                torque_data.launch_control_state = LAUNCHCONTROL_INACTIVE;
            }
            else if (accelerator_position > LAUNCH_CONTROL_THROTTLE_THRESHOLD)
            {
                drv_timer_start(&torque_data.launch_control_timer, LAUNCH_CONTROL_SETTLING_MS);
                torque_data.launch_control_state = LAUNCHCONTROL_SETTLING;
            }
            break;
        case LAUNCHCONTROL_SETTLING:
            if (drv_timer_getState(&torque_data.launch_control_timer) == DRV_TIMER_EXPIRED)
            {
                torque_data.launch_control_state = LAUNCHCONTROL_PRELOAD;
                drv_timer_stop(&torque_data.launch_control_timer);
            }
            else if (accelerator_position < LAUNCH_CONTROL_THROTTLE_THRESHOLD)
            {
                torque_data.launch_control_state = LAUNCHCONTROL_HOLDING;
            }
            else if (brake_position < LAUNCH_CONTROL_BRAKE_THRESHOLD)
            {
                torque_data.launch_control_state = LAUNCHCONTROL_INACTIVE;
            }
            break;
        case LAUNCHCONTROL_PRELOAD:
            if (accelerator_position < LAUNCH_CONTROL_THROTTLE_THRESHOLD)
            {
                torque_data.launch_control_state = LAUNCHCONTROL_INACTIVE;
            }
            else if (brake_position < LAUNCH_CONTROL_BRAKE_THRESHOLD_LAUNCH)
            {
                torque_data.launch_control_state = LAUNCHCONTROL_LAUNCH;
            }
            break;
        case LAUNCHCONTROL_LAUNCH:
            if ((accelerator_position < LAUNCH_CONTROL_THROTTLE_THRESHOLD) ||
                (brake_position > LAUNCH_CONTROL_BRAKE_THRESHOLD))
            {
                torque_data.launch_control_state = LAUNCHCONTROL_INACTIVE;
            }
            break;
        default:
            break;
    }
#else
    UNUSED(accelerator_position);
    UNUSED(brake_position);
#endif
}

static float32_t calc_torque_correction(float32_t target_slip, float32_t actual_slip, float32_t max_torque)
{
    epid_pi_calc(&torque_data.traction_control_pid, target_slip, actual_slip);
    epid_pi_sum(&torque_data.traction_control_pid, 0.0f, max_torque);
    epid_util_ilim(&torque_data.traction_control_pid, 0.0f, TRACTION_CONTROL_ILIM);

    return torque_data.traction_control_pid.y_out;
}

static void evaluate_traction_control(void)
{
    CAN_digitalStatus_E traction_control_requested = CAN_DIGITALSTATUS_SNA;
    float32_t rear_axle_rpm = 0.0f;

    bool requested = (CANRX_get_signal(VEH, SWS_requestTractionControl, &traction_control_requested) != CANRX_MESSAGE_SNA) &&
                      (traction_control_requested == CAN_DIGITALSTATUS_ON);
    bool speed_valid = CANRX_get_signal(VEH, VCREAR_axleRearRPM, &rear_axle_rpm) != CANRX_MESSAGE_SNA;

    if (speed_valid)
    {
        torque_tractionControl_E desired_state = TRACTIONCONTROL_ERROR;
        if (requested)
        {
            if (rear_axle_rpm > TRACTIONCONTROL_RAXLE_THRESHOLD_RPM)
            {
#if FEATURE_IS_ENABLED(FEATURE_TRACTION_CONTROL)
                desired_state = TRACTIONCONTROL_ACTIVE;
#endif
            }
            else
            {
                desired_state = TRACTIONCONTROL_LOCKOUT;
            }
        }
        else
        {
            desired_state = TRACTIONCONTROL_INACTIVE;
        }

        torque_data.traction_control_state = desired_state;
    }
    else
    {
        torque_data.traction_control_state = TRACTIONCONTROL_FAULT_SENSOR;
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
 * @brief Get the max torque request
 * @return Max torque request in Nm
 */
float32_t torque_getTorqueRequestCorrection(void)
{
    return torque_data.torque_correction;
}

/**
 * @brief Get current torque manager state
 * @return CAN state of the torque manager
 */
torque_state_E torque_getState(void)
{
    return torque_data.state;
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
torque_launchControl_E torque_getLaunchControlState(void)
{
    return torque_data.launch_control_state;
}

bool torque_isLaunching(void)
{
    const bool launching = (torque_data.launch_control_state == LAUNCHCONTROL_HOLDING) ||
                           (torque_data.launch_control_state == LAUNCHCONTROL_SETTLING) ||
                           (torque_data.launch_control_state == LAUNCHCONTROL_PRELOAD) ||
                           (torque_data.launch_control_state == LAUNCHCONTROL_LAUNCH);
    return launching;
}

/**
 * @brief Translate launch control state to CAN
 * @return CAN state of the launch control state
 */
CAN_launchControlState_E torque_getLaunchControlStateCAN(void)
{
    CAN_launchControlState_E ret = CAN_LAUNCHCONTROLSTATE_SNA;

    switch (torque_data.launch_control_state)
    {
        case LAUNCHCONTROL_INACTIVE:
            ret = CAN_LAUNCHCONTROLSTATE_INACTIVE;
            break;
        case LAUNCHCONTROL_HOLDING:
            ret = CAN_LAUNCHCONTROLSTATE_HOLDING;
            break;
        case LAUNCHCONTROL_SETTLING:
            ret = CAN_LAUNCHCONTROLSTATE_SETTLING;
            break;
        case LAUNCHCONTROL_PRELOAD:
            ret = CAN_LAUNCHCONTROLSTATE_PRELOAD;
            break;
        case LAUNCHCONTROL_LAUNCH:
            ret = CAN_LAUNCHCONTROLSTATE_LAUNCH;
            break;
        case LAUNCHCONTROL_REJECTED:
            ret = CAN_LAUNCHCONTROLSTATE_REJECTED;
            break;
        case LAUNCHCONTROL_ERROR:
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

static void torque_init(void)
{
    memset(&torque_data, 0x00U, sizeof(torque_data));

    drv_timer_init(&torque_data.torque_change_timer);
    drv_timer_init(&torque_data.launch_control_timer);

    torque_data.state = TORQUE_INACTIVE;
    torque_data.torque_request_max = DEFAULT_BOOT_TORQUE;
    torque_data.gear = GEAR_F;
    torque_data.launch_control_state = LAUNCHCONTROL_INACTIVE;
    torque_data.traction_control_state = TRACTIONCONTROL_INACTIVE;

    epid_init(&torque_data.traction_control_pid, 0.0f, 0.0f, 0.0f,
              TRACTION_CONTROL_KP, TRACTION_CONTROL_KI, 0.0f);
}

static void torque_periodic_100Hz(void)
{
    const float32_t accelerator_position = apps_getPedalPosition();
    const float32_t brake_position = bppc_getPedalPosition();

    torque_data.state = app_vehicleState_getState() == VEHICLESTATE_TS_RUN ? TORQUE_ACTIVE : TORQUE_INACTIVE;
    evaluate_gear_change();
    evaluate_mode_change();
    evaluate_launch_control(accelerator_position, brake_position);
    evaluate_traction_control();

    float32_t torque_request_max = evaluate_torque_max();
    if (torque_data.race_mode != RACEMODE_ENABLED)
    {
        torque_request_max = DEFAULT_TORQUE_PITS;
    }
    if (torque_data.gear != GEAR_F)
    {
        torque_request_max = DEFAULT_TORQUE_LIMIT_REVERSE;
    }

    float32_t torque = (bppc_getState() == BPPC_OK) ?
                        accelerator_position * torque_request_max :
                        0.0f;
    torque_data.torque_correction = calc_torque_correction(TRACTION_CONTROL_TARGET_SLIP,
                                                           TRACTION_CONTROL_TARGET_SLIP,
                                                           torque);
#if FEATURE_IS_ENABLED(FEATURE_TRACTION_CONTROL)
    if (torque_data.traction_control_state == TRACTIONCONTROL_ACTIVE)
    {
        torque -= torque_data.torque_correction;
    }
#endif

    torque_data.torque = SATURATE(ABSOLUTE_MIN_TORQUE, torque, ABSOLUTE_MAX_TORQUE);
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S torque_desc = {
    .moduleInit = &torque_init,
    .periodic100Hz_CLK = &torque_periodic_100Hz,
};
