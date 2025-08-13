/**
 * @file mcManager.c
 * @brief Source file for the motor controller manager
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "mcManager.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "string.h"
#include "MessageUnpack_generated.h"
#include "lib_utility.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define MCMANAGER_TORQUE_LIMIT 130.0f
#define MCMANAGER_FORWARD_DIRECTION MCMANAGER_FORWARD
#define MCMANAGER_REVERSE_DIRECTION ((MCMANAGER_FORWARD_DIRECTION == MCMANAGER_FORWARD) ? MCMANAGER_REVERSE : MCMANAGER_FORWARD)

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    float32_t torque_command;
    mcManager_direction_E direction;
    mcManager_enable_E enable;
    float32_t torque_limit;
    CAN_prechargeContactorState_E last_contactor_state;
    bool clear_faults;
} mcManager_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t mcManager_getTorqueCommand(void)
{
    return mcManager_data.torque_command;
}

CAN_pm100dxDirectionCommand_E mcManager_getDirectionCommand(void)
{
    switch (mcManager_data.direction)
    {
        default:
            return CAN_PM100DXDIRECTIONCOMMAND_REVERSE;
    }
}

CAN_pm100dxEnableState_E mcManager_getEnableCommand(void)
{
    switch (mcManager_data.enable)
    {
        case MCMANAGER_ENABLE:
            return CAN_PM100DXENABLESTATE_ENABLED;
        default:
            return CAN_PM100DXENABLESTATE_DISABLED;
    }
}

float32_t mcManager_getTorqueLimit(void)
{
    return mcManager_data.torque_limit;
}

bool mcManager_clearEepromCommand(void)
{
    const bool ret = mcManager_data.clear_faults;
    mcManager_data.clear_faults = false;

    return ret;
}

static void mcManager_init(void)
{
    memset(&mcManager_data, 0x00, sizeof(mcManager_data));

    mcManager_data.torque_command = 0.0f;
    mcManager_data.torque_limit = MCMANAGER_TORQUE_LIMIT;
    mcManager_data.direction = MCMANAGER_FORWARD_DIRECTION;
    mcManager_data.enable = MCMANAGER_DISABLE;
    mcManager_data.last_contactor_state = CAN_PRECHARGECONTACTORSTATE_SNA;
    mcManager_data.clear_faults = false;
}

static void mcManager_periodic_100Hz(void)
{
    float32_t torque_command = 0.0f;
    mcManager_enable_E enable = MCMANAGER_DISABLE;
    CAN_prechargeContactorState_E contactor_state = CAN_PRECHARGECONTACTORSTATE_SNA;

    (void)CANRX_get_signal(VEH, BMSB_packContactorState, &contactor_state);

    switch (app_vehicleState_getState())
    {
        case VEHICLESTATE_TS_RUN:
            {
                CAN_torqueManagerState_E manager_state = CAN_TORQUEMANAGERSTATE_SNA;

                const bool command_valid = CANRX_get_signal(VEH, VCFRONT_torqueRequest, &torque_command) == CANRX_MESSAGE_VALID;
                (void)CANRX_get_signal(VEH, VCFRONT_torqueManagerState, &manager_state);

                if (command_valid && (manager_state == CAN_TORQUEMANAGERSTATE_ACTIVE))
                {
                    enable = MCMANAGER_ENABLE;
                    break;
                }
                torque_command = 0.0f;
                enable = MCMANAGER_DISABLE;
            }
            break;
        case VEHICLESTATE_ON_HV:
            {
                if ((mcManager_data.last_contactor_state != CAN_PRECHARGECONTACTORSTATE_HVP_CLOSED) &&
                    (contactor_state == CAN_PRECHARGECONTACTORSTATE_HVP_CLOSED))
                {
                    mcManager_data.clear_faults = true;
                }
                break;
            }
        default:
            break;
    }

    mcManager_data.last_contactor_state = contactor_state;
    mcManager_data.enable = enable;
    mcManager_data.torque_command = SATURATE(0.0f, torque_command, MCMANAGER_TORQUE_LIMIT);
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S mcManager_desc = {
    .moduleInit = &mcManager_init,
    .periodic100Hz_CLK = &mcManager_periodic_100Hz,
};
