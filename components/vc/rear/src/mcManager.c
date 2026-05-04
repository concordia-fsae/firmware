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

#include "lib_utility.h"
#include "lib_rateLimit.h"
#include "lib_simpleFilter.h"
#include "drv_timer.h"
#include "app_faultManager.h"
#include "drv_timer.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    REQUEST_PARAMETER = 0x00U,
    READ_PARAMETER,
    PAUSE,
    SPIN_MOTOR,
    SAMPLE,
} calibrationPhase_E;

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CALIBRATION_PAUSE_TIME_MS 2000U
#define CALIBRATION_TORQUE_REQUEST_NM 5
#define MCMANAGER_TORQUE_LIMIT 180.0f
#define MCMANAGER_TORQUE_LIMIT_REVERSE 25.0f

#define LASH_TORQUE 2.0f
#define LASH_TORQUE_RPM_DISABLE 180.0f
#define LASH_TORQUE_RPM_ENABLE 2 * LASH_TORQUE_RPM_DISABLE

#define DRIVETRAIN_MULTIPLIER 4.6f

#define RAMPRATE_NM_PER_S 1000

#define MOTOR_BACKWARDS true
#define MC_COMMAND_REVERSE (MOTOR_BACKWARDS ? CAN_PM100DXDIRECTIONCOMMAND_FORWARD : CAN_PM100DXDIRECTIONCOMMAND_REVERSE)
#define MC_COMMAND_FORWARD (MOTOR_BACKWARDS ? CAN_PM100DXDIRECTIONCOMMAND_REVERSE : CAN_PM100DXDIRECTIONCOMMAND_FORWARD)

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    lib_rateLimit_linear_S torque_command;
    mcManager_direction_E direction;
    mcManager_enable_E enable;
    float32_t torque_limit;
    CAN_prechargeContactorState_E last_contactor_state;
    bool clear_faults;
    bool lash_enabled;
    bool calibrating;

    uint16_t axle_rpm;
} mcManager_data;

static struct
{
    uint8_t                    attempts;
    calibrationPhase_E         state;
    float32_t                  angleConfigured;
    lib_simpleFilter_cumAvgF_S deltaFilteredMeasuredAvg;
    drv_timer_S                timerPause;
} calibrationData;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t mcManager_getTorqueCommand(void)
{
    return mcManager_data.torque_command.y_n;
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

    const bool lockout_valid = CANRX_get_signal(VEH, PM100DX_inverterEnableLockout, &inverter_lock_out) == CANRX_MESSAGE_VALID;

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
    return mcManager_data.torque_limit;
}

bool mcManager_startResolverCalibration(void)
{
    const bool eepromActive = CANRX_validate(VEH, PM100DX_eepromResponse) == CANRX_MESSAGE_VALID;
    bool ret = false;

    if (!mcManager_data.calibrating && !mcManager_data.clear_faults &&
        (app_vehicleState_getState() == VEHICLESTATE_ON_HV) &&
        !eepromActive)
    {
        ret = true;
        mcManager_data.calibrating = true;
        calibrationData.state = REQUEST_PARAMETER;
        calibrationData.attempts = 0U;
    }

    return ret;
}

bool mcManager_isResolverCalibrating(void)
{
    return mcManager_data.calibrating;
}

bool mcManager_requestContactorsOpen(void)
{
    return mcManager_data.calibrating && (calibrationData.state == SAMPLE);
}

static void mcManager_init(void)
{
    memset(&mcManager_data, 0x00, sizeof(mcManager_data));

    drv_timer_init(&calibrationData.timerPause);
    mcManager_data.torque_command.y_n = 0.0f;
    mcManager_data.torque_command.maxStepDelta = RAMPRATE_NM_PER_S / 100;
    mcManager_data.torque_limit = 0.0f;
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
    const bool speed_valid = CANRX_get_signal(VEH, PM100DX_motorSpeedCritical, &motor_rpm) == CANRX_MESSAGE_VALID;
    const bool miaBms = CANRX_get_signal(VEH, BMSB_packContactorState, &contactor_state) != CANRX_MESSAGE_VALID;
    app_faultManager_setFaultState(FM_FAULT_VCREAR_MIAMC, !speed_valid);

    bool mcFaulted = app_faultManager_getNetworkedFault_anySet(VEH, PM100DX_faults);
    app_faultManager_setFaultState(FM_FAULT_VCREAR_MCFAULTED, mcFaulted);
    const bool miaFront = CANRX_validate(VEH, VCFRONT_torqueManager) != CANRX_MESSAGE_VALID;
    const bool miaPdu = CANRX_validate(VEH, VCPDU_vehicleState) != CANRX_MESSAGE_VALID;
    app_faultManager_setFaultState(FM_FAULT_VCREAR_MIAVCFRONT, miaFront);
    app_faultManager_setFaultState(FM_FAULT_VCREAR_MIAVCPDU, miaPdu);
    app_faultManager_setFaultState(FM_FAULT_VCREAR_MIABMS, miaBms);
    app_faultManager_setFaultState(FM_FAULT_VCREAR_MCCALIBRATINGRESOLVER, mcManager_data.calibrating);

    const bool motorSpinningPhysicallyForward = motor_rpm > 0;
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
                if (mcManager_data.clear_faults)
                {
                    const bool injectPending = CANTX_inject_pending(VEH, TOOLING_mcEepromCommand);

                    if (!injectPending)
                    {
                        CAN_data_T message = { 0 };
                        set(&message,VEH,TOOLING,eepromAddress, CAN_PM100DXEEPROMADDRESS_FAULT_CLEAR);
                        set(&message,VEH,TOOLING,eepromCommand, CAN_PM100DXEEPROMRWCOMMAND_WRITE);
                        set(&message,VEH,TOOLING,eepromDataRaw, 0U);
                        (void)CANTX_inject(VEH, TOOLING_mcEepromCommand, &message);

                        // TODO: Only clear faults when they actually clear
                        mcManager_data.clear_faults = false;
                    }
                }
                break;
            }
        default:
            break;
    }

    // Verify calibration is still valid
    if (mcManager_data.calibrating)
    {
        if ((app_vehicleState_getState() != VEHICLESTATE_ON_HV) ||
            (calibrationData.attempts > 3U))
        {
            calibrationData.state = REQUEST_PARAMETER;
            mcManager_data.calibrating = false;
            app_faultManager_setFaultState(FM_FAULT_VCREAR_MCCALIBRATINGRESOLVERFAILED, true);
        }
    }

    if (mcManager_data.calibrating)
    {
        switch (calibrationData.state)
        {
            case REQUEST_PARAMETER:
                {
                    drv_timer_start(&calibrationData.timerPause, CALIBRATION_PAUSE_TIME_MS);
                    const bool injectPending = CANTX_inject_pending(VEH, TOOLING_mcEepromCommand);

                    if (!injectPending)
                    {
                        CAN_data_T message = { 0 };
                        set(&message,VEH,TOOLING,eepromAddress, CAN_PM100DXEEPROMADDRESS_GAMMA_ADJUST_EEPROM);
                        set(&message,VEH,TOOLING,eepromCommand, CAN_PM100DXEEPROMRWCOMMAND_READ);
                        set(&message,VEH,TOOLING,eepromDataRaw, 0U);
                        (void)CANTX_inject(VEH, TOOLING_mcEepromCommand, &message);

                        calibrationData.state = READ_PARAMETER;
                    }
                }
                break;
            case READ_PARAMETER:
                {
                    const bool eepromActive = CANRX_validate(VEH, PM100DX_eepromResponse) == CANRX_MESSAGE_VALID;

                    if (eepromActive)
                    {
                        uint8_t parameter = 0x00U;
                        int16_t val = 0x00U;
                        (void)CANRX_get_signal(VEH, PM100DX_eepromAddress, &parameter);
                        (void)CANRX_get_signal(VEH, PM100DX_eepromDataRaw, (uint16_t*)&val);

                        calibrationData.angleConfigured = ((float32_t)val) / 10.0f;
                        calibrationData.state = PAUSE;
                        mcManager_data.torque_command.y_n = 0.0f;
                    }
                }
                break;
            case PAUSE:
                if (drv_timer_getState(&calibrationData.timerPause) == DRV_TIMER_EXPIRED)
                {
                    calibrationData.state = SPIN_MOTOR;
                }
                break;
            case SPIN_MOTOR:
                {
                    lib_simpleFilter_cumAvgF_clear(&calibrationData.deltaFilteredMeasuredAvg);
                    enable = MCMANAGER_ENABLE;
                    if ((contactor_state == CAN_PRECHARGECONTACTORSTATE_HVP_CLOSED) && (mcManager_data.torque_command.y_n < CALIBRATION_TORQUE_REQUEST_NM))
                    {
                        torque_command = mcManager_data.torque_command.y_n;
                        torque_command += 1;
                    }

                    if (motor_rpm > 1500)
                    {
                        calibrationData.state = SAMPLE;
                        mcManager_data.torque_command.y_n = 0.0f;
                    }
                }
                break;
            case SAMPLE:
                {
                    float32_t deltaResolver = 0;
                    (void)CANRX_get_signal(VEH, PM100DX_deltaResolverFiltered, &deltaResolver);

                    if (motor_rpm < 1000)
                    {
                        const float32_t targetDelta = motorSpinningPhysicallyForward ? 90.0f : -90.0f;
                        const float32_t measuredDelta = lib_simpleFilter_cumAvgF_average(&calibrationData.deltaFilteredMeasuredAvg);
                        const float32_t calibrationDelta = targetDelta - measuredDelta;

                        if (calibrationData.deltaFilteredMeasuredAvg.count < 5)
                        {
                            calibrationData.state = SPIN_MOTOR;
                            break;
                        }

                        if ((calibrationDelta < 0.5) && (calibrationDelta > -0.5))
                        {
                            calibrationData.state = REQUEST_PARAMETER;
                            mcManager_data.calibrating = false;
                            app_faultManager_setFaultState(FM_FAULT_VCREAR_MCCALIBRATINGRESOLVERFAILED, false);
                            break;
                        }
                        else
                        {
                            const bool injectPending = CANTX_inject_pending(VEH, TOOLING_mcEepromCommand);
                            const int16_t config = (int16_t)((calibrationDelta + calibrationData.angleConfigured) * 10);

                            if (!injectPending && (contactor_state == CAN_PRECHARGECONTACTORSTATE_OPEN))
                            {
                                CAN_data_T message = { 0 };
                                set(&message,VEH,TOOLING,eepromAddress, CAN_PM100DXEEPROMADDRESS_GAMMA_ADJUST_EEPROM);
                                set(&message,VEH,TOOLING,eepromCommand, CAN_PM100DXEEPROMRWCOMMAND_WRITE);
                                set(&message,VEH,TOOLING,eepromDataRaw, (uint32_t)config);
                                (void)CANTX_inject(VEH, TOOLING_mcEepromCommand, &message);
                                calibrationData.state = SPIN_MOTOR;
                                calibrationData.angleConfigured += calibrationDelta;
                                calibrationData.attempts++;
                            }
                        }
                    }
                    else if ((motor_rpm < 1100) && (contactor_state == CAN_PRECHARGECONTACTORSTATE_OPEN))
                    {
                        lib_simpleFilter_cumAvgF_increment(&calibrationData.deltaFilteredMeasuredAvg, deltaResolver);
                    }
                }
            default:
                break;
        }
    }

#if FEATURE_IS_ENABLED(FEATURE_REVERSE)
    CAN_gear_E direction = CAN_GEAR_FORWARD;
    const bool direction_valid = CANRX_get_signal(VEH, VCFRONT_gear, &direction) == CANRX_MESSAGE_VALID;
    if (direction_valid && (direction == CAN_GEAR_REVERSE) && !mcManager_data.calibrating)
    {
        mcManager_data.torque_limit = enable == MCMANAGER_ENABLE ? MCMANAGER_TORQUE_LIMIT_REVERSE : 0.0f;
        mcManager_data.direction = MCMANAGER_REVERSE;
    }
    else
#endif
    {
        float32_t activeTorque = !mcManager_data.calibrating ? MCMANAGER_TORQUE_LIMIT : CALIBRATION_TORQUE_REQUEST_NM;
        mcManager_data.torque_limit = enable == MCMANAGER_ENABLE ? activeTorque : 0.0f;
        mcManager_data.direction = MCMANAGER_FORWARD;
    }

    const bool isRegenTorque = torque_command < 0.0f;
    const float32_t min_torque = mcManager_data.lash_enabled ? (!isRegenTorque ? LASH_TORQUE : -LASH_TORQUE) : 0.0f;
    const float32_t maxLimit = !isRegenTorque ? mcManager_data.torque_limit : min_torque;
    const float32_t minLimit = !isRegenTorque ? min_torque : -mcManager_data.torque_limit;

    mcManager_data.last_contactor_state = contactor_state;
    mcManager_data.enable = enable;
    torque_command = SATURATE(minLimit, torque_command, maxLimit);
    lib_rateLimit_linear_update(&mcManager_data.torque_command, torque_command);
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S mcManager_desc = {
    .moduleInit = &mcManager_init,
    .periodic100Hz_CLK = &mcManager_periodic_100Hz,
};
