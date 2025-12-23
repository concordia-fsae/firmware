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
    bool race_mode_change_active;

    drv_timer_S torque_change_timer;

    torque_launchControl_E launch_control_state;
} torque_data;

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

    torque_data.state = TORQUE_INACTIVE;
    torque_data.torque_request_max = DEFAULT_BOOT_TORQUE;
    torque_data.gear = GEAR_F;
    torque_data.launch_control_state = LAUNCHCONTROL_INACTIVE;
}

static void torque_periodic_100Hz(void)
{
    CAN_digitalStatus_E gear_change_request = CAN_DIGITALSTATUS_SNA;
    CAN_digitalStatus_E race_mode_change_requested = CAN_DIGITALSTATUS_SNA;
    float32_t torque_request_max = torque_data.torque_request_max;
    float32_t torque = 0.0f;

    const bool gear_change_was_requested = torque_data.gear_change_active;
    torque_data.gear_change_active = (CANRX_get_signal(VEH, SWS_requestReverse, &gear_change_request) != CANRX_MESSAGE_SNA) &&
                                     (gear_change_request == CAN_DIGITALSTATUS_ON);
    const bool gear_change_rising = !gear_change_was_requested && torque_data.gear_change_active;

    const bool race_mode_change_was_requested = torque_data.race_mode_change_active;
    torque_data.race_mode_change_active = (CANRX_get_signal(VEH, SWS_requestRaceMode, &race_mode_change_requested) != CANRX_MESSAGE_SNA) &&
                                          (race_mode_change_requested == CAN_DIGITALSTATUS_ON);
    const bool race_mode_change_rising = !race_mode_change_was_requested && torque_data.race_mode_change_active;

    CAN_digitalStatus_E torque_change_request = CAN_DIGITALSTATUS_SNA;
    const bool torque_inc_active = (CANRX_get_signal(VEH, SWS_requestTorqueInc, &torque_change_request) != CANRX_MESSAGE_SNA) &&
                                   (torque_change_request == CAN_DIGITALSTATUS_ON);
    const bool torque_dec_active = (CANRX_get_signal(VEH, SWS_requestTorqueDec, &torque_change_request) != CANRX_MESSAGE_SNA) &&
                                   (torque_change_request == CAN_DIGITALSTATUS_ON);

#if FEATURE_IS_ENABLED(FEATURE_REVERSE)
    if (gear_change_rising)
    {
        torque_data.gear = torque_data.gear == GEAR_F ? GEAR_R : GEAR_F;
    }
#endif

    if (race_mode_change_rising)
    {
        torque_data.race_mode = torque_data.race_mode == RACEMODE_ENABLED ? RACEMODE_PIT : RACEMODE_ENABLED;
    }

    torque_data.state = app_vehicleState_getState() == VEHICLESTATE_TS_RUN ? TORQUE_ACTIVE : TORQUE_INACTIVE;

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

    if (torque_data.race_mode != RACEMODE_ENABLED)
    {
        torque_request_max = DEFAULT_TORQUE_PITS;
    }

    if (torque_data.gear != GEAR_F)
    {
        torque_request_max = DEFAULT_TORQUE_LIMIT_REVERSE;
    }

    torque = (bppc_getState() == BPPC_OK) ?
              apps_getPedalPosition() * torque_request_max :
              0.0f;

    torque_data.torque = SATURATE(ABSOLUTE_MIN_TORQUE, torque, ABSOLUTE_MAX_TORQUE);
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S torque_desc = {
    .moduleInit = &torque_init,
    .periodic100Hz_CLK = &torque_periodic_100Hz,
};
