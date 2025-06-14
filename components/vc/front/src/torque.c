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

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define DEFAULT_TORQUE_PITS 10.0f // 10Nm on boot

#define ABSOLUTE_MAX_TORQUE 130.0f
#define ABSOLUTE_MIN_TORQUE 0.0f

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

static struct
{
    torque_state_E state;
    float32_t      torque;
    float32_t      torque_request_max;
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
    return (torque_data.state == TORQUE_ACTIVE) ? torque_data.torque : 0.0f;
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

static void torque_init(void)
{
    memset(&torque_data, 0x00U, sizeof(torque_data));

    torque_data.state = TORQUE_INACTIVE;
    torque_data.torque_request_max = DEFAULT_TORQUE_PITS;
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

    torque = (bppc_getState() == BPPC_OK) ?
              apps_getPedalPosition() * torque_data.torque_request_max :
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
