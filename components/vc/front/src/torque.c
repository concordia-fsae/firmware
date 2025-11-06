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
#include "pid.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define DEFAULT_TORQUE_PITS 25.0f
#define DEFAULT_TORQUE_LIMIT_REVERSE 15.0f

#define ABSOLUTE_MAX_TORQUE 130.0f
#define ABSOLUTE_MIN_TORQUE 0.0f

#define EPID_KP  0.0f
#define EPID_KI  0.0f
#define EPID_KD  0.0f
#define DEADBAND 0.0f /* Off==0 */

#define START_PID_INPUT 0.0f

#define PID_LIM_MIN ABSOLUTE_MIN_TORQUE
#define PID_LIM_MAX ABSOLUTE_MAX_TORQUE

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

static struct
{
    torque_state_E state;
    torque_state_E traction_control;
    float32_t      torque;
    float32_t      torque_request_max;

    bool traction_control_requested;

#if FEATURE_IS_ENABLED(FEATURE_TRACTION_CONTROL)
    epid_t tc_pid_ctx;
    float32_t deadband_delta;
#endif
} torque_data;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static CAN_torqueManagerState_E translateTorqueStateToCAN(torque_state_E state)
{
    CAN_torqueManagerState_E ret = CAN_TORQUEMANAGERSTATE_SNA;

    switch (state)
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

#if FEATURE_IS_ENABLED(FEATURE_TRACTION_CONTROL)
static float32_t modulateTorqueForTargetTraction(float32_t driver_request)
{
    CAN_digitalStatus_E traction_control_request = CAN_DIGITALSTATUS_SNA;
    torque_data.traction_control_requested = (CANRX_get_signal(VEH, SWS_requestTractionControl, &traction_control_request) != CANRX_MESSAGE_SNA) &&
                                             (traction_control_request == CAN_DIGITALSTATUS_ON);

    if (torque_data.traction_control == TORQUE_ACTIVE)
    {
        if (!torque_data.traction_control_requested)
        {
            torque_data.traction_control = TORQUE_INACTIVE;
        }
        else
        {
            epid_pid_calc(&torque_data.tc_pid_ctx, START_PID_INPUT, START_PID_INPUT); /* Calc PID terms values */

            /* Apply deadband filter to `delta[k]`. */
            torque_data.deadband_delta = torque_data.tc_pid_ctx.p_term + torque_data.tc_pid_ctx.i_term + torque_data.tc_pid_ctx.d_term;
            if (fabsf(torque_data.deadband_delta) >= DEADBAND)
            {
                /* Compute new control signal output */
                epid_pid_sum(&torque_data.tc_pid_ctx, PID_LIM_MIN, PID_LIM_MAX);
                driver_request = torque_data.tc_pid_ctx.y_out;
            }
        }
    }
    else if (torque_data.traction_control == TORQUE_INACTIVE)
    {
        epid_info_t epid_err = epid_init(&torque_data.tc_pid_ctx,
                                         START_PID_INPUT, START_PID_INPUT, PID_LIM_MIN,
                                         EPID_KP, EPID_KI, EPID_KD);

        if (epid_err != EPID_ERR_NONE)
        {
            torque_data.traction_control = TORQUE_ERROR;
        }
        else if (torque_data.traction_control_requested)
        {
            torque_data.traction_control = TORQUE_ACTIVE;
        }
    }

    return driver_request;
}
#endif

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
 * @brief Get current torque manager state
 * @return CAN state of the torque manager
 */
torque_state_E torque_getState(void)
{
    return torque_data.state;
}

/**
 * @brief Translate torque state to CAN
 * @return CAN state of the torque manager
 */
CAN_torqueManagerState_E torque_getStateCAN(void)
{
    return translateTorqueStateToCAN(torque_data.state);
}

/**
 * @brief Get current traction control state
 * @return CAN state of the torque manager traction control
 */
torque_state_E torque_getTractionControlState(void)
{
    return torque_data.traction_control;
}

/**
 * @brief Translate traction control state to CAN
 * @return CAN state of the torque manager traction control
 */
CAN_torqueManagerState_E torque_getTractionControlStateCAN(void)
{
    return translateTorqueStateToCAN(torque_data.traction_control);
}

static void torque_init(void)
{
    memset(&torque_data, 0x00U, sizeof(torque_data));

    torque_data.state = TORQUE_INACTIVE;
    torque_data.torque_request_max = DEFAULT_TORQUE_PITS;
    torque_data.traction_control = TORQUE_INACTIVE;
}

static void torque_periodic_100Hz(void)
{
    CAN_raceMode_E race_mode = CAN_RACEMODE_PIT;
    float32_t torque = 0.0f;
    const bool race_mode_not_sna = CANRX_get_signal(VEH, STW_raceMode, &race_mode) != CANRX_MESSAGE_SNA;

    if (app_vehicleState_getState() == VEHICLESTATE_TS_RUN)
    {
        torque_data.state = TORQUE_ACTIVE;
    }
    else
    {
        torque_data.state = TORQUE_INACTIVE;
    }
    if (race_mode_not_sna)
    {
        if (race_mode == CAN_RACEMODE_RACE)
        {
            float32_t max_torque = 0.0f;
            (void)CANRX_get_signal(VEH, STW_maxRequestTorque, &max_torque);

            torque_data.torque_request_max = max_torque;
        }
        else
        {
            torque_data.torque_request_max = DEFAULT_TORQUE_PITS;
        }
    }

#if FEATURE_IS_ENABLED(FEATURE_REVERSE)
    CAN_gear_E direction = CAN_GEAR_FORWARD;
    const bool direction_valid = CANRX_get_signal(VEH, STW_gear, &direction) == CANRX_MESSAGE_VALID;
    if (direction_valid && (direction == CAN_GEAR_REVERSE))
    {
        torque_data.torque_request_max = DEFAULT_TORQUE_LIMIT_REVERSE;
    }
#endif

    torque = (bppc_getState() == BPPC_OK) ?
              apps_getPedalPosition() * torque_data.torque_request_max :
              0.0f;

#if FEATURE_IS_ENABLED(FEATURE_TRACTION_CONTROL)
    torque = modulateTorqueForTargetTraction(torque);
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
