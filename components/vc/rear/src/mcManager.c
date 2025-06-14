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
            return CAN_PM100DXDIRECTIONCOMMAND_FORWARD;
    }
}

CAN_pm100dxEnableState_E mcManager_getEnableCommand(void)
{
    switch (mcManager_data.direction)
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

static void mcManager_init(void)
{
    memset(&mcManager_data, 0x00, sizeof(mcManager_data));

    mcManager_data.torque_command = 0.0f;
    mcManager_data.torque_limit = MCMANAGER_TORQUE_LIMIT;
    mcManager_data.direction = MCMANAGER_FORWARD_DIRECTION;
    mcManager_data.enable = MCMANAGER_DISABLE;
}

static void mcManager_periodic_100Hz(void)
{
    float32_t torque_command = 0.0f;
    mcManager_enable_E enable = MCMANAGER_DISABLE;

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
        default:
            break;
    }

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
