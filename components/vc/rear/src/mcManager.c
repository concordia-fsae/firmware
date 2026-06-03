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

#include "app_faultManager.h"
#include "drv_tempSensors.h"
#include "drv_timer.h"
#include "drv_timer.h"
#include "lib_rateLimit.h"
#include "lib_simpleFilter.h"
#include "lib_utility.h"
#include "Yamcan.h"

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

typedef enum
{
    EEPROM_PARAMETER_FLUX_WEAKENING_CURRENT = 0x00U,
    EEPROM_PARAMETER_GAMMA_ADJUST,
    EEPROM_PARAMETER_COUNT,
} eepromParameter_E;

typedef enum
{
    EEPROM_PARAMETER_UNIT_AMP = 0x00U,
    EEPROM_PARAMETER_UNIT_DEGREE,
    EEPROM_PARAMETER_UNIT_COUNT,
} eepromParameterUnit_E;

typedef struct
{
    CAN_pm100dxEepromAddress_E address;
    eepromParameterUnit_E      unit;
    bool                       clampValue;
    float32_t                  minValue;
    float32_t                  maxValue;
} eepromParameterConfig_S;

typedef struct
{
    float32_t rawMultiplier;
} eepromParameterUnitConfig_S;

typedef struct
{
    int16_t                      savedRaw;
    int16_t                      requestedRaw;
    int16_t                      transmittedRaw;
    CAN_pm100dxEepromRWCommand_E transmittedCommand;
    bool                         savedValid;
    bool                         requestValid;
    bool                         readRequested;
    bool                         writeRequested;
    bool                         transmitPending;
    uint8_t                      updateCount;
} eepromParameterState_S;

typedef struct
{
    CAN_pm100dxEepromAddress_E   address;
    CAN_pm100dxEepromRWCommand_E command;
    int16_t                      rawData;
    bool                         queued;
} eepromCommandQueue_S;

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CALIBRATION_PAUSE_TIME_MS          2000U
#define CALIBRATION_SPIN_TIMEOUT_MS        10000U
#define CALIBRATION_TORQUE_REQUEST_NM      10
#define MCMANAGER_TORQUE_LIMIT             180.0f
#define MCMANAGER_TORQUE_LIMIT_REVERSE     25.0f

#define LASH_TORQUE                        2.0f
#define LASH_TORQUE_RPM_DISABLE            180.0f
#define LASH_TORQUE_RPM_ENABLE             2 * LASH_TORQUE_RPM_DISABLE

#define DRIVETRAIN_MULTIPLIER              4.6f

#define RAMPRATE_NM_PER_S                  1000

#define MOTOR_BACKWARDS                    true
#define MC_COMMAND_REVERSE                 (MOTOR_BACKWARDS ? CAN_PM100DXDIRECTIONCOMMAND_FORWARD : CAN_PM100DXDIRECTIONCOMMAND_REVERSE)
#define MC_COMMAND_FORWARD                 (MOTOR_BACKWARDS ? CAN_PM100DXDIRECTIONCOMMAND_REVERSE : CAN_PM100DXDIRECTIONCOMMAND_FORWARD)

#define FLUX_WEAKENING_CURRENT_MIN_A       0.0f
#define FLUX_WEAKENING_CURRENT_MAX_A       225.0f
#define FLUX_WEAKENING_CURRENT_STEP_A      1.0f
#define FLUX_WEAKENING_CURRENT_DELAY_MS    250U

#define EEPROM_BOOT_READ_DELAY_MS          500U

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    lib_rateLimit_linear_S        torque_command;
    mcManager_direction_E         direction;
    mcManager_enable_E            enable;
    float32_t                     torque_limit;
    CAN_prechargeContactorState_E last_contactor_state;
    bool                          clear_faults;
    bool                          lash_enabled;
    bool                          calibrating;
    bool                          testCalibration;

    uint16_t                      axle_rpm;
    float32_t                     tempTsCap;
    float32_t                     vcRear_torqueLimitCurrent;
} mcManager_data;

static struct
{
    uint8_t                    attempts;
    calibrationPhase_E         state;
    float32_t                  angleConfigured;
    lib_simpleFilter_cumAvgF_S deltaFilteredMeasuredAvg;
    drv_timer_S                timerPause;
    uint8_t                    parameterUpdateCount;
} calibrationData;

static struct
{
    bool        init;
    drv_timer_S timerChange;
}                                        currentFluxWeakening;

static eepromParameterState_S            eepromParameters[EEPROM_PARAMETER_COUNT];
static eepromCommandQueue_S              eepromCommandQueue;
static drv_timer_S                       eepromBootReadDelay;

static const eepromParameterUnitConfig_S eepromParameterUnitConfigs[EEPROM_PARAMETER_UNIT_COUNT] = {
    [EEPROM_PARAMETER_UNIT_AMP]    = {
        .rawMultiplier = 10.0f,
    },
    [EEPROM_PARAMETER_UNIT_DEGREE] = {
        .rawMultiplier = 10.0f,
    },
};

static const eepromParameterConfig_S     eepromParameterConfigs[EEPROM_PARAMETER_COUNT] = {
    [EEPROM_PARAMETER_FLUX_WEAKENING_CURRENT] = {
        .address    = CAN_PM100DXEEPROMADDRESS_CURRENT_MAX_ID,
        .unit       = EEPROM_PARAMETER_UNIT_AMP,
        .clampValue = true,
        .minValue   = FLUX_WEAKENING_CURRENT_MIN_A,
        .maxValue   = FLUX_WEAKENING_CURRENT_MAX_A,
    },
    [EEPROM_PARAMETER_GAMMA_ADJUST] =           {
        .address    = CAN_PM100DXEEPROMADDRESS_GAMMA_ADJUST_EEPROM,
        .unit       = EEPROM_PARAMETER_UNIT_DEGREE,
        .clampValue = false,
        .minValue   =                                         0.0f,
        .maxValue   =                                       359.9f,
    },
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static int16_t eepromValueToRaw(const eepromParameter_E parameter, float32_t value)
{
    const eepromParameterConfig_S * const     config = &eepromParameterConfigs[parameter];
    const eepromParameterUnitConfig_S * const unit   = &eepromParameterUnitConfigs[config->unit];

    if (config->clampValue)
    {
        value = SATURATE(config->minValue, value, config->maxValue);
    }

    return (int16_t)(value * unit->rawMultiplier);
}

static float32_t eepromRawToValue(const eepromParameter_E parameter, const int16_t raw)
{
    const eepromParameterConfig_S * const     config = &eepromParameterConfigs[parameter];
    const eepromParameterUnitConfig_S * const unit   = &eepromParameterUnitConfigs[config->unit];
    float32_t                                 value  = ((float32_t)raw) / unit->rawMultiplier;

    if (config->clampValue)
    {
        value = SATURATE(config->minValue, value, config->maxValue);
    }

    return value;
}

static bool sendEepromCommand(const CAN_pm100dxEepromAddress_E   address,
                              const CAN_pm100dxEepromRWCommand_E command,
                              const int16_t                      rawData)
{
    if (CANTX_inject_pending(VEH, TOOLING_mcEepromCommand))
    {
        return false;
    }

    CAN_data_T message = { 0 };
    set(&message, VEH, TOOLING, eepromAddress, address);
    set(&message, VEH, TOOLING, eepromCommand, command);
    set(&message, VEH, TOOLING, eepromDataRaw, (uint32_t)(uint16_t)rawData);

    return CANTX_inject(VEH, TOOLING_mcEepromCommand, &message);
}

static bool getEepromResponse(CAN_pm100dxEepromAddress_E * const   address,
                              CAN_pm100dxEepromRWCommand_E * const command,
                              int16_t * const                      rawData)
{
    const bool eepromActive = CANRX_validate(VEH, PM100DX_eepromResponse) == CANRX_MESSAGE_VALID;

    if (!eepromActive)
    {
        return false;
    }

    CAN_pm100dxEepromAddress_E   parameter     = (CAN_pm100dxEepromAddress_E)0U;
    CAN_pm100dxEepromRWCommand_E eepromCommand = CAN_PM100DXEEPROMRWCOMMAND_READ;
    uint16_t                     data          = 0U;
    (void)CANRX_get_signal(VEH, PM100DX_eepromAddress, &parameter);
    (void)CANRX_get_signal(VEH, PM100DX_eepromCommand, &eepromCommand);
    (void)CANRX_get_signal(VEH, PM100DX_eepromDataRaw, &data);

    if ((uint32_t)parameter == 0U)
    {
        return false;
    }

    if (address != NULL)
    {
        *address = parameter;
    }
    if (command != NULL)
    {
        *command = eepromCommand;
    }
    if (rawData != NULL)
    {
        *rawData = (int16_t)data;
    }

    return true;
}

static bool eepromFindParameter(const CAN_pm100dxEepromAddress_E address, eepromParameter_E * const parameter)
{
    for (uint8_t i = 0U; i < (uint8_t)EEPROM_PARAMETER_COUNT; i++)
    {
        const eepromParameter_E candidate = (eepromParameter_E)i;
        if (eepromParameterConfigs[candidate].address == address)
        {
            *parameter = candidate;
            return true;
        }
    }

    return false;
}

static void eepromApplyParameter(const eepromParameter_E parameter)
{
    switch (parameter)
    {
        case EEPROM_PARAMETER_GAMMA_ADJUST:
            calibrationData.angleConfigured = eepromRawToValue(EEPROM_PARAMETER_GAMMA_ADJUST,
                                                               eepromParameters[EEPROM_PARAMETER_GAMMA_ADJUST].savedRaw);
            break;

        case EEPROM_PARAMETER_FLUX_WEAKENING_CURRENT:
        case EEPROM_PARAMETER_COUNT:
            break;
    }
}

static void eepromHandleResponses(void)
{
    CAN_pm100dxEepromAddress_E   address   = (CAN_pm100dxEepromAddress_E)0U;
    CAN_pm100dxEepromRWCommand_E command   = CAN_PM100DXEEPROMRWCOMMAND_READ;
    int16_t                      rawData   = 0;
    eepromParameter_E            parameter = EEPROM_PARAMETER_COUNT;

    if (!getEepromResponse(&address, &command, &rawData) ||
        !eepromFindParameter(address, &parameter)
        )
    {
        return;
    }

    eepromParameterState_S * const state                   = &eepromParameters[parameter];
    const bool                     responseMatchesTransmit = state->transmitPending &&
                                                             (state->transmittedCommand == command) &&
                                                             ((command == CAN_PM100DXEEPROMRWCOMMAND_READ) ||
                                                              (rawData == state->transmittedRaw));

    state->savedRaw   = rawData;
    state->savedValid = true;
    state->updateCount++;
    eepromApplyParameter(parameter);

    if (!state->requestValid)
    {
        state->requestedRaw = rawData;
        state->requestValid = true;
    }

    if (responseMatchesTransmit)
    {
        state->transmitPending = false;
        if (command == CAN_PM100DXEEPROMRWCOMMAND_READ)
        {
            state->readRequested = false;
        }
    }
    state->writeRequested = state->requestValid &&
                            (state->savedRaw != state->requestedRaw) &&
                            !state->transmitPending;
}

static void eepromTransmitPending(void)
{
    for (uint8_t i = 0U; i < (uint8_t)EEPROM_PARAMETER_COUNT; i++)
    {
        if (eepromParameters[(eepromParameter_E)i].transmitPending)
        {
            return;
        }
    }

    if (eepromCommandQueue.queued)
    {
        if (sendEepromCommand(eepromCommandQueue.address,
                              eepromCommandQueue.command,
                              eepromCommandQueue.rawData)
            )
        {
            eepromCommandQueue.queued = false;
        }
        return;
    }

    for (uint8_t i = 0U; i < (uint8_t)EEPROM_PARAMETER_COUNT; i++)
    {
        const eepromParameter_E        parameter = (eepromParameter_E)i;
        eepromParameterState_S * const state     = &eepromParameters[parameter];

        if (state->requestValid && state->writeRequested)
        {
            if (sendEepromCommand(eepromParameterConfigs[parameter].address,
                                  CAN_PM100DXEEPROMRWCOMMAND_WRITE,
                                  state->requestedRaw)
                )
            {
                state->transmitPending    = true;
                state->transmittedCommand = CAN_PM100DXEEPROMRWCOMMAND_WRITE;
                state->transmittedRaw     = state->requestedRaw;
                state->writeRequested     = false;
            }
            return;
        }
    }

    if (drv_timer_getState(&eepromBootReadDelay) == DRV_TIMER_RUNNING)
    {
        return;
    }

    for (uint8_t i = 0U; i < (uint8_t)EEPROM_PARAMETER_COUNT; i++)
    {
        const eepromParameter_E        parameter = (eepromParameter_E)i;
        eepromParameterState_S * const state     = &eepromParameters[parameter];

        if (state->readRequested)
        {
            if (sendEepromCommand(eepromParameterConfigs[parameter].address,
                                  CAN_PM100DXEEPROMRWCOMMAND_READ,
                                  0)
                )
            {
                state->transmitPending    = true;
                state->transmittedCommand = CAN_PM100DXEEPROMRWCOMMAND_READ;
                state->transmittedRaw     = 0;
            }
            return;
        }
    }
}

static void updateFluxWeakeningCurrentRequest(const int16_t requestSum)
{
    if (requestSum == 0)
    {
        drv_timer_stop(&currentFluxWeakening.timerChange);
        return;
    }

    const drv_timer_state_E timerState = drv_timer_getState(&currentFluxWeakening.timerChange);
    if (timerState == DRV_TIMER_RUNNING)
    {
        return;
    }
    if (timerState == DRV_TIMER_EXPIRED)
    {
        drv_timer_stop(&currentFluxWeakening.timerChange);
        return;
    }

    eepromParameterState_S * const state = &eepromParameters[EEPROM_PARAMETER_FLUX_WEAKENING_CURRENT];

    if (!state->savedValid)
    {
        return;
    }

    const int16_t rawRequested = state->requestValid ? state->requestedRaw : state->savedRaw;
    const int16_t rawNext      = eepromValueToRaw(EEPROM_PARAMETER_FLUX_WEAKENING_CURRENT,
                                                  eepromRawToValue(EEPROM_PARAMETER_FLUX_WEAKENING_CURRENT, rawRequested) +
                                                  (FLUX_WEAKENING_CURRENT_STEP_A * (float32_t)requestSum));

    if (rawNext == rawRequested)
    {
        drv_timer_start(&currentFluxWeakening.timerChange, FLUX_WEAKENING_CURRENT_DELAY_MS);
        return;
    }

    state->requestedRaw   = rawNext;
    state->requestValid   = true;
    state->writeRequested = state->savedRaw != state->requestedRaw;
    drv_timer_start(&currentFluxWeakening.timerChange, FLUX_WEAKENING_CURRENT_DELAY_MS);
}

static void evaluateFluxWeakeningCurrent(void)
{
    CAN_digitalStatus_E request    = CAN_DIGITALSTATUS_SNA;
    const bool          requestInc = (CANRX_get_signal(VEH, SWS_requestFluxWeakeningCurrentInc, &request) == CANRX_MESSAGE_VALID) &&
                                     (request == CAN_DIGITALSTATUS_ON);
    const bool          requestDec = (CANRX_get_signal(VEH, SWS_requestFluxWeakeningCurrentDec, &request) == CANRX_MESSAGE_VALID) &&
                                     (request == CAN_DIGITALSTATUS_ON);
    const int16_t       requestSum = (int16_t)((requestInc ? 1 : 0) - (requestDec ? 1 : 0));

    if (!mcManager_data.clear_faults && !mcManager_data.calibrating)
    {
        updateFluxWeakeningCurrentRequest(requestSum);
    }
}

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
    CAN_pm100dxEnableState_E          ret               = CAN_PM100DXENABLESTATE_DISABLED;
    CAN_pm100dxInverterLockoutState_E inverter_lock_out = CAN_PM100DXINVERTERLOCKOUTSTATE_CANNOT_BE_ENABLED;

    const bool                        lockout_valid     = CANRX_get_signal(VEH, PM100DX_inverterEnableLockout, &inverter_lock_out) == CANRX_MESSAGE_VALID;

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

float32_t mcManager_getTsCapTemperatureDegC(void)
{
    return mcManager_data.tempTsCap;
}

uint8_t mcManager_getResolverCalibrationAttempts(void)
{
    return calibrationData.attempts;
}

float32_t mcManager_getResolverCalibrationConfiguredAngleDeg(void)
{
    return calibrationData.angleConfigured;
}

float32_t mcManager_getResolverCalibrationDeltaFilteredMeasuredDeg(void)
{
    return lib_simpleFilter_cumAvgF_average(&calibrationData.deltaFilteredMeasuredAvg);
}

float32_t mcManager_getFluxWeakeningCurrent(void)
{
    return eepromParameters[EEPROM_PARAMETER_FLUX_WEAKENING_CURRENT].savedValid ?
           eepromRawToValue(EEPROM_PARAMETER_FLUX_WEAKENING_CURRENT,
                            eepromParameters[EEPROM_PARAMETER_FLUX_WEAKENING_CURRENT].savedRaw) :
           0.0f;
}
float32_t mcManager_getvcRearTorqueLimit(void)
{
    return mcManager_data.vcRear_torqueLimitCurrent;
}

bool mcManager_startResolverCalibration(void)
{
    bool eepromBusy = eepromCommandQueue.queued || CANTX_inject_pending(VEH, TOOLING_mcEepromCommand);
    bool ret        = false;

    for (uint8_t i = 0U; i < (uint8_t)EEPROM_PARAMETER_COUNT; i++)
    {
        eepromBusy |= eepromParameters[(eepromParameter_E)i].transmitPending;
    }

    if (!mcManager_data.calibrating && !mcManager_data.clear_faults &&
        (app_vehicleState_getState() == VEHICLESTATE_ON_HV) &&
        !eepromBusy
        )
    {
        ret                             = true;
        mcManager_data.calibrating      = true;
        calibrationData.state           = REQUEST_PARAMETER;
        calibrationData.attempts        = 0U;
        calibrationData.angleConfigured = 0.0f;
        lib_simpleFilter_cumAvgF_clear(&calibrationData.deltaFilteredMeasuredAvg);
    }

    return ret;
}

bool mcManager_testResolverCalibration(void)
{
    const bool success = mcManager_startResolverCalibration();

    if (success)
    {
        mcManager_data.testCalibration = true;
    }

    return success;
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
    memset(&mcManager_data,       0x00, sizeof(mcManager_data));
    memset(&currentFluxWeakening, 0x00, sizeof(currentFluxWeakening));
    memset(eepromParameters,      0x00, sizeof(eepromParameters));
    memset(&eepromCommandQueue,   0x00, sizeof(eepromCommandQueue));

    drv_timer_init(&calibrationData.timerPause);
    drv_timer_init(&currentFluxWeakening.timerChange);
    drv_timer_init(&eepromBootReadDelay);
    drv_timer_start(&eepromBootReadDelay, EEPROM_BOOT_READ_DELAY_MS);
    mcManager_data.torque_command.y_n                                       = 0.0f;
    mcManager_data.torque_command.maxStepDelta                              = RAMPRATE_NM_PER_S / 100;
    mcManager_data.torque_limit                                             = 0.0f;
    mcManager_data.direction                                                = MCMANAGER_FORWARD;
    mcManager_data.enable                                                   = MCMANAGER_DISABLE;
    mcManager_data.last_contactor_state                                     = CAN_PRECHARGECONTACTORSTATE_SNA;
    mcManager_data.clear_faults                                             = false;

    eepromParameters[EEPROM_PARAMETER_FLUX_WEAKENING_CURRENT].readRequested = true;
    mcManager_data.vcRear_torqueLimitCurrent                                = 0.0;
}

static void mcManager_periodic_100Hz(void)
{
    float32_t                     torque_command    = 0.0f;
    mcManager_enable_E            enable            = MCMANAGER_DISABLE;
    CAN_prechargeContactorState_E contactor_state   = CAN_PRECHARGECONTACTORSTATE_SNA;
    int16_t                       motor_rpm         = 0;
    bool                          speed_valid       = false;
    bool                          miaBms            = false;
    bool                          discharge_valid   = false;
    bool                          voltage_valid     = false;
    float32_t                     BMSB_maxDischarge = 0.0f;
    float32_t                     BMSB_voltage      = 0.0f;

    eepromHandleResponses();

    speed_valid     = CANRX_get_signal(VEH, PM100DX_motorSpeedCritical, &motor_rpm) == CANRX_MESSAGE_VALID;
    miaBms          = CANRX_get_signal(VEH, BMSB_packContactorState, &contactor_state) != CANRX_MESSAGE_VALID;
    discharge_valid = CANRX_get_signal(VEH, BMSB_maxDischarge, &BMSB_maxDischarge) == CANRX_MESSAGE_VALID;
    voltage_valid   = CANRX_get_signal(VEH, BMSB_packVoltage, &BMSB_voltage) == CANRX_MESSAGE_VALID;
    app_faultManager_setFaultState(FM_FAULT_VCREAR_MIAMC, !speed_valid);

    bool       mcFaulted = app_faultManager_getNetworkedFault_anySet(VEH, PM100DX_faults);
    app_faultManager_setFaultState(FM_FAULT_VCREAR_MCFAULTED, mcFaulted);
    const bool miaFront  = CANRX_validate(VEH, VCFRONT_torqueManager) != CANRX_MESSAGE_VALID;
    const bool miaPdu    = CANRX_validate(VEH, VCPDU_vehicleState) != CANRX_MESSAGE_VALID;
    app_faultManager_setFaultState(FM_FAULT_VCREAR_MIAVCFRONT,            miaFront);
    app_faultManager_setFaultState(FM_FAULT_VCREAR_MIAVCPDU,              miaPdu);
    app_faultManager_setFaultState(FM_FAULT_VCREAR_MIABMS,                miaBms);
    app_faultManager_setFaultState(FM_FAULT_VCREAR_MCCALIBRATINGRESOLVER, mcManager_data.calibrating);
    mcManager_data.tempTsCap = drv_tempSensors_getChannelTemperatureDegC(DRV_TEMPSENSORS_CHANNEL_TS_CAP);

    motor_rpm                = (int16_t)((motor_rpm < 0) ? -motor_rpm : motor_rpm);
    mcManager_data.axle_rpm  = (uint16_t)(motor_rpm / DRIVETRAIN_MULTIPLIER);
    if (speed_valid && discharge_valid && voltage_valid && (motor_rpm > 0))
    {
        mcManager_data.vcRear_torqueLimitCurrent = BMSB_voltage * BMSB_maxDischarge / (RPM_TO_RAD_P_S((float32_t)motor_rpm));
    }
    else
    {
        mcManager_data.vcRear_torqueLimitCurrent = MCMANAGER_TORQUE_LIMIT;
    }
    switch (app_vehicleState_getState())
    {
        case VEHICLESTATE_TS_RUN:
        {
            CAN_torqueManagerState_E manager_state = CAN_TORQUEMANAGERSTATE_SNA;

            const bool               command_valid = CANRX_get_signal(VEH, VCFRONT_torqueRequest, &torque_command) == CANRX_MESSAGE_VALID;
            (void)CANRX_get_signal(VEH, VCFRONT_torqueManagerState, &manager_state);

            if (!mcManager_data.lash_enabled)
            {
                if ((speed_valid &&
                     ((motor_rpm > 2 * LASH_TORQUE_RPM_ENABLE) ||
                      (motor_rpm < 2 * -LASH_TORQUE_RPM_ENABLE))) &&
                    (torque_command > LASH_TORQUE)
                    )
                {
                    mcManager_data.lash_enabled = true;
                }
            }
            else if (!speed_valid ||
                     ((motor_rpm < LASH_TORQUE_RPM_DISABLE) &&
                      (motor_rpm > -LASH_TORQUE_RPM_DISABLE))
                     )
            {
                mcManager_data.lash_enabled = false;
            }

            if (command_valid && (manager_state == CAN_TORQUEMANAGERSTATE_ACTIVE))
            {
                enable = MCMANAGER_ENABLE;
                break;
            }
            torque_command = 0.0f;
            enable         = MCMANAGER_DISABLE;
        }
        break;

        case VEHICLESTATE_ON_HV:
        {
            mcManager_data.lash_enabled = false;
            if ((mcManager_data.last_contactor_state != CAN_PRECHARGECONTACTORSTATE_HVP_CLOSED) &&
                (contactor_state == CAN_PRECHARGECONTACTORSTATE_HVP_CLOSED)
                )
            {
                mcManager_data.clear_faults = true;
            }
            if (mcManager_data.clear_faults)
            {
                if (!eepromCommandQueue.queued)
                {
                    eepromCommandQueue.address  = CAN_PM100DXEEPROMADDRESS_FAULT_CLEAR;
                    eepromCommandQueue.command  = CAN_PM100DXEEPROMRWCOMMAND_WRITE;
                    eepromCommandQueue.rawData  = 0;
                    eepromCommandQueue.queued   = true;
                    mcManager_data.clear_faults = false;
                }
            }
            break;
        }

        default:
            break;
    }

    evaluateFluxWeakeningCurrent();

    // Verify calibration is still valid
    if (mcManager_data.calibrating)
    {
        if ((app_vehicleState_getState() != VEHICLESTATE_ON_HV) ||
            (calibrationData.attempts > (3U + (mcManager_data.testCalibration ? 1U : 0U)))
            )
        {
            calibrationData.state          = REQUEST_PARAMETER;
            mcManager_data.calibrating     = false;
            mcManager_data.testCalibration = false;
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
                calibrationData.parameterUpdateCount                          = eepromParameters[EEPROM_PARAMETER_GAMMA_ADJUST].updateCount;
                eepromParameters[EEPROM_PARAMETER_GAMMA_ADJUST].readRequested = true;
                calibrationData.state                                         = READ_PARAMETER;
            }
            break;

            case READ_PARAMETER:
            {
                if (eepromParameters[EEPROM_PARAMETER_GAMMA_ADJUST].updateCount != calibrationData.parameterUpdateCount)
                {
                    calibrationData.state             = PAUSE;
                    mcManager_data.torque_command.y_n = 0.0f;
                }
            }
            break;

            case PAUSE:
                if (drv_timer_getState(&calibrationData.timerPause) == DRV_TIMER_EXPIRED)
                {
                    drv_timer_start(&calibrationData.timerPause, CALIBRATION_SPIN_TIMEOUT_MS);
                    calibrationData.state = SPIN_MOTOR;
                }
                break;

            case SPIN_MOTOR:
            {
                if (drv_timer_getState(&calibrationData.timerPause) == DRV_TIMER_EXPIRED)
                {
                    drv_timer_stop(&calibrationData.timerPause);
                    calibrationData.state      = REQUEST_PARAMETER;
                    mcManager_data.calibrating = false;
                    app_faultManager_setFaultState(FM_FAULT_VCREAR_MCCALIBRATINGRESOLVERFAILED, true);
                    break;
                }
                lib_simpleFilter_cumAvgF_clear(&calibrationData.deltaFilteredMeasuredAvg);
                enable = MCMANAGER_ENABLE;
                if ((contactor_state == CAN_PRECHARGECONTACTORSTATE_HVP_CLOSED) && (mcManager_data.torque_command.y_n < CALIBRATION_TORQUE_REQUEST_NM))
                {
                    torque_command  = mcManager_data.torque_command.y_n;
                    torque_command += 1;
                }

                if (motor_rpm > 1500)
                {
                    calibrationData.state             = SAMPLE;
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
                    const float32_t targetDelta      = MOTOR_BACKWARDS ? -90.0f : 90.0f;
                    const float32_t measuredDelta    = lib_simpleFilter_cumAvgF_average(&calibrationData.deltaFilteredMeasuredAvg);
                    const float32_t calibrationDelta = measuredDelta - targetDelta;

                    if (calibrationData.deltaFilteredMeasuredAvg.count < 5)
                    {
                        calibrationData.state = SPIN_MOTOR;
                        break;
                    }

                    if ((calibrationDelta < 0.5) && (calibrationDelta > -0.5))
                    {
                        if (mcManager_data.testCalibration)
                        {
                            calibrationData.state          = REQUEST_PARAMETER;
                            mcManager_data.calibrating     = false;
                            mcManager_data.testCalibration = false;
                            app_faultManager_setFaultState(FM_FAULT_VCREAR_MCCALIBRATINGRESOLVERFAILED, false);
                        }
                        else
                        {
                            mcManager_data.testCalibration = true;
                            calibrationData.state          = SPIN_MOTOR;
                            calibrationData.attempts++;
                        }
                        break;
                    }
                    else if (mcManager_data.testCalibration)
                    {
                        calibrationData.state          = REQUEST_PARAMETER;
                        mcManager_data.calibrating     = false;
                        mcManager_data.testCalibration = false;
                        app_faultManager_setFaultState(FM_FAULT_VCREAR_MCCALIBRATINGRESOLVERFAILED, true);
                        break;
                    }
                    else
                    {
                        eepromParameterState_S * const state            = &eepromParameters[EEPROM_PARAMETER_GAMMA_ADJUST];
                        const bool                     writeOutstanding = state->writeRequested ||
                                                                          (state->transmitPending &&
                                                                           (state->transmittedCommand == CAN_PM100DXEEPROMRWCOMMAND_WRITE));
                        const int16_t                  config           = eepromValueToRaw(EEPROM_PARAMETER_GAMMA_ADJUST,
                                                                                           calibrationDelta + calibrationData.angleConfigured);

                        if (!writeOutstanding && (contactor_state == CAN_PRECHARGECONTACTORSTATE_OPEN))
                        {
                            state->requestedRaw              = config;
                            state->requestValid              = true;
                            state->writeRequested            = !state->savedValid || (state->savedRaw != state->requestedRaw);
                            calibrationData.state            = SPIN_MOTOR;
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
    CAN_gear_E direction       = CAN_GEAR_FORWARD;
    const bool direction_valid = CANRX_get_signal(VEH, VCFRONT_gear, &direction) == CANRX_MESSAGE_VALID;
    if (direction_valid && (direction == CAN_GEAR_REVERSE) && !mcManager_data.calibrating)
    {
        mcManager_data.torque_limit = (enable == MCMANAGER_ENABLE) ? MCMANAGER_TORQUE_LIMIT_REVERSE : 0.0f;
        mcManager_data.direction    = MCMANAGER_REVERSE;
    }
    else
#endif
    {
        float32_t activeTorque = !mcManager_data.calibrating ? MCMANAGER_TORQUE_LIMIT : CALIBRATION_TORQUE_REQUEST_NM;
        mcManager_data.torque_limit = (enable == MCMANAGER_ENABLE) ? activeTorque : 0.0f;
        mcManager_data.direction    = MCMANAGER_FORWARD;
    }

    const bool      isRegenTorque = torque_command < 0.0f;
    const float32_t min_torque    = mcManager_data.lash_enabled ? (!isRegenTorque ? LASH_TORQUE : -LASH_TORQUE) : 0.0f;
    const float32_t maxLimit      = !isRegenTorque ? mcManager_data.torque_limit : min_torque;
    const float32_t minLimit      = !isRegenTorque ? min_torque : -mcManager_data.torque_limit;

    mcManager_data.last_contactor_state = contactor_state;
    mcManager_data.enable               = enable;
    const float32_t effectiveMaxLimit = (!isRegenTorque && (mcManager_data.vcRear_torqueLimitCurrent < maxLimit)) ? mcManager_data.vcRear_torqueLimitCurrent : maxLimit;
    torque_command                      = SATURATE(minLimit, torque_command, effectiveMaxLimit);
    lib_rateLimit_linear_update(&mcManager_data.torque_command, torque_command);

    eepromTransmitPending();
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S mcManager_desc = {
    .moduleInit        = &mcManager_init,
    .periodic100Hz_CLK = &mcManager_periodic_100Hz,
};
