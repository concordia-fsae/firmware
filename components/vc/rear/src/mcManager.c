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
#define MCMANAGER_TORQUE_LIMIT_REVERSE 15.0f

#define LASH_TORQUE 2.0f
#define LASH_TORQUE_RPM_DISABLE 180.0f
#define LASH_TORQUE_RPM_ENABLE 2 * LASH_TORQUE_RPM_DISABLE

#define DRIVETRAIN_MULTIPLIER 4.6f

#define MOTOR_BACKWARDS true
#define MC_COMMAND_REVERSE (MOTOR_BACKWARDS ? CAN_PM100DXDIRECTIONCOMMAND_FORWARD : CAN_PM100DXDIRECTIONCOMMAND_REVERSE)
#define MC_COMMAND_FORWARD (MOTOR_BACKWARDS ? CAN_PM100DXDIRECTIONCOMMAND_REVERSE : CAN_PM100DXDIRECTIONCOMMAND_FORWARD)

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
    bool lash_enabled;

    uint16_t axle_rpm;
} mcManager_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t mcManager_getTorqueCommand(void)
{
    return mcManager_data.torque_command;
}

float32_t mcManager_getAxleRPM(void)
{
    return mcManager_data.axle_rpm;
}

CAN_pm100dxDirectionCommand_E mcManager_getDirectionCommand(void)
{
    switch (mcManager_data.direction)
    {
        case MCMANAGER_REVERSE:
            return MC_COMMAND_REVERSE;
        default:
            return MC_COMMAND_FORWARD;
    }
}

CAN_pm100dxEnableState_E mcManager_getEnableCommand(void)
{
    CAN_pm100dxEnableState_E ret = CAN_PM100DXENABLESTATE_DISABLED;
    CAN_pm100dxInverterLockoutState_E inverter_lock_out = CAN_PM100DXINVERTERLOCKOUTSTATE_CANNOT_BE_ENABLED;

    const bool lockout_valid = CANRX_get_signal(ASS, PM100DX_inverterEnableLockout, &inverter_lock_out) == CANRX_MESSAGE_VALID;

    if (!lockout_valid || (inverter_lock_out == CAN_PM100DXINVERTERLOCKOUTSTATE_CANNOT_BE_ENABLED))
    {
        return CAN_PM100DXENABLESTATE_DISABLED;
    }

    switch (mcManager_data.enable)
    {
        case MCMANAGER_ENABLE:
            ret = CAN_PM100DXENABLESTATE_ENABLED;
            break;
        default:
            ret = CAN_PM100DXENABLESTATE_DISABLED;
            break;
    }

    return ret;
}

float32_t mcManager_getTorqueLimit(void)
{
    if (mcManager_data.direction == MCMANAGER_REVERSE)
    {
        return MCMANAGER_TORQUE_LIMIT_REVERSE;
    }

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
    mcManager_data.direction = MCMANAGER_FORWARD;
    mcManager_data.enable = MCMANAGER_DISABLE;
    mcManager_data.last_contactor_state = CAN_PRECHARGECONTACTORSTATE_SNA;
    mcManager_data.clear_faults = false;
}

static void mcManager_periodic_100Hz(void)
{
    float32_t torque_command = 0.0f;
    mcManager_enable_E enable = MCMANAGER_DISABLE;
    CAN_prechargeContactorState_E contactor_state = CAN_PRECHARGECONTACTORSTATE_SNA;
    int16_t motor_rpm = 0;
    const bool speed_valid = CANRX_get_signal(ASS, PM100DX_motorSpeedCritical, &motor_rpm) == CANRX_MESSAGE_VALID;
    (void)CANRX_get_signal(VEH, BMSB_packContactorState, &contactor_state);

    motor_rpm = (int16_t)(motor_rpm < 0 ? -motor_rpm : motor_rpm);
    mcManager_data.axle_rpm = (uint16_t)(motor_rpm / DRIVETRAIN_MULTIPLIER);

    switch (app_vehicleState_getState())
    {
        case VEHICLESTATE_TS_RUN:
            {
                CAN_torqueManagerState_E manager_state = CAN_TORQUEMANAGERSTATE_SNA;

                const bool command_valid = CANRX_get_signal(VEH, VCFRONT_torqueRequest, &torque_command) == CANRX_MESSAGE_VALID;
                (void)CANRX_get_signal(VEH, VCFRONT_torqueManagerState, &manager_state);

                if (!mcManager_data.lash_enabled)
                {
                    if ((speed_valid && 
                        ((motor_rpm > 2 * LASH_TORQUE_RPM_ENABLE) ||
                         (motor_rpm < 2 * -LASH_TORQUE_RPM_ENABLE))) &&
                        (torque_command > LASH_TORQUE))
                    {
                        mcManager_data.lash_enabled = true;
                    }
                }
                else if (!speed_valid ||
                         ((motor_rpm < LASH_TORQUE_RPM_DISABLE) &&
                          (motor_rpm > -LASH_TORQUE_RPM_DISABLE)))
                {
                    mcManager_data.lash_enabled = false;
                }

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
                mcManager_data.lash_enabled = false;
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

#if FEATURE_IS_ENABLED(FEATURE_REVERSE)
    CAN_gear_E direction = CAN_GEAR_FORWARD;
    const bool direction_valid = CANRX_get_signal(VEH, VCFRONT_gear, &direction) == CANRX_MESSAGE_VALID;
    if (direction_valid && (direction == CAN_GEAR_REVERSE))
    {
        mcManager_data.direction = MCMANAGER_REVERSE;
        torque_command = SATURATE(0.0f, torque_command, MCMANAGER_TORQUE_LIMIT_REVERSE);
    }
    else
    {
        mcManager_data.direction = MCMANAGER_FORWARD;
    }
#endif

    const float32_t min_torque = mcManager_data.lash_enabled ? LASH_TORQUE : 0.0f;

    mcManager_data.last_contactor_state = contactor_state;
    mcManager_data.enable = enable;
    mcManager_data.torque_command = SATURATE(min_torque, torque_command, MCMANAGER_TORQUE_LIMIT);
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S mcManager_desc = {
    .moduleInit = &mcManager_init,
    .periodic100Hz_CLK = &mcManager_periodic_100Hz,
};
