/**
 * @file torque.c
 * @brief Torque manager source code for vehicle control
 * @note Units for torque are in Nm
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "app_faultManager.h"
#include "app_vehicleSpeed.h"
#include "app_vehicleState.h"
#include "apps.h"
#include "bppc.h"
#include "FeatureDefines.h"
#include "lib_utility.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "string.h"
#include "torque.h"
#include "vd.h"

#include "drv_timer.h"
#include "lib_pid.h"
#include "lib_rateLimit.h"
#include "Yamcan.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define DEFAULT_BOOT_TORQUE              130.0f
#define DEFAULT_TORQUE_PITS              25.0f
#define DEFAULT_TORQUE_LIMIT_REVERSE     20.0f

#define ABSOLUTE_MAX_TORQUE              175.0f
#define ABSOLUTE_MIN_TORQUE              0.0f
#define MIN_TORQUE_RANGE                 90.0f
#define MAX_TORQUE_NM_PER_S              500
#define MAX_LAUNCH_NM_PER_S              1000
#define PRELOAD_NM_PER_S                 100
#define GEAR_RATIO                       4.6f

#define TORQUE_CHANGE_DELAY              250

#define LC_PRELOAD_TORQUE_INIT           30
#define LC_PRELOAD_TORQUE_MIN            10
#define LC_PRELOAD_TORQUE_MAX            75
#define LC_SETTLING_MS                   500
#define LC_REJECTED_MS                   2000
#define LC_BRAKE_THRESHOLD               0.30
#define LC_BRAKE_THRESHOLD_LAUNCH        0.05
#define LC_THROTTLE_THRESHOLD            0.15
#define LC_THROTTLE_START_CUTOFF         0.10
#define LC_75M_DISTANCE_KM               0.075f
#define LC_75M_TIMEOUT_MS                5000U

#define REGEN_BRAKE_START_THRESH         0.10f
#define REGEN_BRAKE_STOP_THRESH          0.05f
#define REGEN_ACCEL_CUTOFF               0.15f
#define REGEN_MAX_TORQUE_N               50
#define REGEN_SPEED_CUTOFF_MPS           1.38f // Rules limitation to regen

#define TC_MIN                           0.0f
#define TC_TARGET_SLIP                   0.10f
#define TC_VEHICLESPEED_THRESHOLD_MPS    VEHICLE_STOPPED_THRESHOLD
#define TC_PARAM_ACTIVITY_TIMEOUT_MS     30000
#define TC_PARAM_CHANGE_DELAY_MS         250

#define ABSOLUTE_MIN_SLIP                0.05f
#define ABSOLUTE_MAX_SLIP                0.30f
#define SLIP_TARGET_STEP                 0.01f
#define SLIP_CHANGE_DELAY                250

#define PEDAL_SLEEP_THRESHOLD            0.02f
#define SLEEP_TIMEOUT_MS                 15 * 60000

#define PEDAL_APPLIED_THRESHOLD          0.10f
#define VEHICLE_STOPPED_THRESHOLD        0.2

#define CAR_MASS                         285.0f
#define WHEEL_DIAMETER                   0.4064f
#define WHEEL_BASE                       1.543f
#define CG_HEIGHT                        0.350f
#define EFFECTIVE_ROTOR_RADIUS           0.0769f
#define CAR_WEIGHT                       2795.0f
#define REAR_CALIPER_AREA                0.00045239f


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

static struct
{
    torque_state_E                state;
    torque_gear_E                 gear;
    torque_raceMode_E             race_mode;
    float32_t                     torque;
    float32_t                     regenTorque;
    float32_t                     torquePreload;
    float32_t                     torqueDriverInput;
    float32_t                     torque_request_max;
    float32_t                     slip_request;
    lib_rateLimit_linear_S        torqueRateLimit;
    lib_rateLimit_linear_S        launchRateLimit;
    lib_rateLimit_linear_S        preloadRateLimit;

    bool                          gear_change_active;
    bool                          torque_control_request_active;
    bool                          race_mode_change_active;
    bool                          regenEnabled;
    bool                          isRegenerating;
    bool                          customTcPidWritePending;
    bool                          tcMappingIncActive;
    bool                          tcMappingDecActive;
    drv_timer_S                   torque_change_timer;
    drv_timer_S                   launch_control_timer;
    drv_timer_S                   launch_75m_timer;
    drv_timer_S                   preloadChangeTimer;
    drv_timer_S                   slip_change_timer;
    drv_timer_S                   paramTcSaveTimer;
    drv_timer_S                   paramTcChangeTimer;
    nvm_tcPid_S                   tcPid100Nm;
    nvm_tcPid_S                   tcPid150Nm;

    torque_launchControlState_E   launchControlState;
    torque_tractionControlState_E tractionControlState;
    float32_t                     launch75mStartOdometerKm;
    float32_t                     launch75mTime;

    uint32_t                      lastTimeampMS;
    float32_t                     slipRear;
    float32_t                     torqueCorrection;
    float32_t                     torqueReduction;
    float32_t                     maxVdTorque;
    lib_pid_S                     tractionControlPID;

    FLAG_create(tcParamsWasRequested, PARAMSTATE_COUNT);
} torque_data;

typedef enum
{
    PARAMVALUE_TC_KP = 0x00U,
    PARAMVALUE_TC_KI,
    PARAMVALUE_TC_KD,
    PARAMVALUE_TC_MAX_LIM,
    PARAMVALUE_TC_ILIM,
    PARAMVALUE_TC_TLEAK_MS,
    PARAMVALUE_COUNT,
} paramConfig_E;

typedef struct
{
    CANRX_MESSAGE_health_E (*requestInc)(CAN_digitalStatus_E* status);
    CANRX_MESSAGE_health_E (*requestDec)(CAN_digitalStatus_E* status);
} paramValueConfig_S;

static paramValueConfig_S     paramValues[PARAMVALUE_COUNT] = {
    [PARAMVALUE_TC_KP] =       {
        .requestInc = CANRX_get_signal_func(VEH, SWS_requestTcKpInc),
        .requestDec = CANRX_get_signal_func(VEH, SWS_requestTcKpDec),
    },
    [PARAMVALUE_TC_KI] =       {
        .requestInc = CANRX_get_signal_func(VEH, SWS_requestTcKiInc),
        .requestDec = CANRX_get_signal_func(VEH, SWS_requestTcKiDec),
    },
    [PARAMVALUE_TC_KD] =       {
        .requestInc = CANRX_get_signal_func(VEH, SWS_requestTcKdInc),
        .requestDec = CANRX_get_signal_func(VEH, SWS_requestTcKdDec),
    },
    [PARAMVALUE_TC_MAX_LIM] =  {
        .requestInc = CANRX_get_signal_func(VEH, SWS_requestTcMaxLimInc),
        .requestDec = CANRX_get_signal_func(VEH, SWS_requestTcMaxLimDec),
    },
    [PARAMVALUE_TC_ILIM] =     {
        .requestInc = CANRX_get_signal_func(VEH, SWS_requestTcILimInc),
        .requestDec = CANRX_get_signal_func(VEH, SWS_requestTcILimDec),
    },
    [PARAMVALUE_TC_TLEAK_MS] = {
        .requestInc = CANRX_get_signal_func(VEH, SWS_requestTcTLeakMsInc),
        .requestDec = CANRX_get_signal_func(VEH, SWS_requestTcTLeakMsDec),
    },
};

static CANRX_MESSAGE_health_E (*paramRequestStateCanSignal[PARAMSTATE_COUNT])(CAN_digitalStatus_E* status) = {
    [PARAMSTATE_TC_TIRE_MODEL_LIMIT] = CANRX_get_signal_func(VEH, SWS_requestTcTireModelLimit),
};

TC_SET_100NM_PID(static const nvm_tcPid_S tcPid100NmDefault);
TC_SET_150NM_PID(static const nvm_tcPid_S tcPid150NmDefault);

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void launch_control_75m_reset(void)
{
    torque_data.launch75mStartOdometerKm = 0.0f;
    torque_data.launch75mTime            = 0.0f;
    drv_timer_stop(&torque_data.launch_75m_timer);
}

static void launch_control_75m_start(void)
{
    torque_data.launch75mStartOdometerKm = app_vehicleSpeed_getOdometer();
    torque_data.launch75mTime            = 0.0f;
    drv_timer_start(&torque_data.launch_75m_timer, LC_75M_TIMEOUT_MS);
}

static void launch_control_75m_set_time(float32_t time_s)
{
    torque_data.launch75mTime = SATURATE(0.0f, time_s, 5.0f);
}

static void launch_control_75m_update(void)
{
    const app_vehicleState_state_E vehicle_state = app_vehicleState_getState();
    const bool                     hv_active     = (vehicle_state == VEHICLESTATE_ON_HV) ||
                                                   (vehicle_state == VEHICLESTATE_TS_RUN);

    if (!hv_active)
    {
        launch_control_75m_reset();
        return;
    }

    const drv_timer_state_E timer_state = drv_timer_getState(&torque_data.launch_75m_timer);
    if (timer_state == DRV_TIMER_STOPPED)
    {
        return;
    }

    const uint32_t elapsed_ms = drv_timer_getElapsedTimeMs(&torque_data.launch_75m_timer);
    if (timer_state == DRV_TIMER_EXPIRED)
    {
        launch_control_75m_set_time(5.0f);
        if (torque_data.launchControlState == LC_STATE_LAUNCH)
        {
            torque_data.launchControlState = LC_STATE_INACTIVE;
        }
        drv_timer_stop(&torque_data.launch_75m_timer);
        return;
    }

    const float32_t elapsed_s   = ((float32_t)elapsed_ms) / 1000.0f;
    const float32_t distance_km = app_vehicleSpeed_getOdometer() - torque_data.launch75mStartOdometerKm;
    launch_control_75m_set_time(elapsed_s);
    if (distance_km >= LC_75M_DISTANCE_KM)
    {
        drv_timer_stop(&torque_data.launch_75m_timer);
    }
}

static tc_mapping_E tc_getSelectedMapping(void)
{
    if (tcParamState_data.selectedTcMapping >= TC_MAPPING_COUNT)
    {
        return TC_MAPPING_CUSTOM;
    }

    return (tc_mapping_E)tcParamState_data.selectedTcMapping;
}

static bool tc_isCustomMappingSelected(void)
{
    return tc_getSelectedMapping() == TC_MAPPING_CUSTOM;
}

static nvm_tcPid_S* tc_getActivePid(void)
{
    nvm_tcPid_S* pid = &tcPid_data;

    switch (tc_getSelectedMapping())
    {
        case TC_MAPPING_100NM:
            pid = &torque_data.tcPid100Nm;
            break;

        case TC_MAPPING_150NM:
            pid = &torque_data.tcPid150Nm;
            break;

        case TC_MAPPING_CUSTOM:
        default:
            break;
    }

    return pid;
}

static const nvm_tcPid_S* tc_getActivePidConst(void)
{
    return tc_getActivePid();
}

static void tc_requestCustomPidWriteAfterActivity(void)
{
    if (tc_isCustomMappingSelected())
    {
        torque_data.customTcPidWritePending = true;
        drv_timer_start(&torque_data.paramTcSaveTimer, TC_PARAM_ACTIVITY_TIMEOUT_MS);
    }
}

static void tc_setSelectedMapping(tc_mapping_E mapping)
{
    if (mapping >= TC_MAPPING_COUNT)
    {
        mapping = TC_MAPPING_CUSTOM;
    }

    if (tc_getSelectedMapping() != mapping)
    {
        tcParamState_data.selectedTcMapping = (uint16_t)mapping;
        lib_nvm_requestWrite(NVM_ENTRYID_TC_PARAMSTATE);
    }

    torque_data.torque_request_max = (float32_t)tc_getActivePidConst()->maxTorqueNm;
}

static tc_mapping_E tc_stepMapping(tc_mapping_E mapping, int16_t step)
{
    if (step > 0)
    {
        switch (mapping)
        {
            case TC_MAPPING_CUSTOM:
                return TC_MAPPING_150NM;

            case TC_MAPPING_100NM:
                return TC_MAPPING_150NM;

            case TC_MAPPING_150NM:
            default:
                return TC_MAPPING_CUSTOM;
        }
    }

    if (step < 0)
    {
        switch (mapping)
        {
            case TC_MAPPING_CUSTOM:
                return TC_MAPPING_100NM;

            case TC_MAPPING_150NM:
                return TC_MAPPING_100NM;

            case TC_MAPPING_100NM:
            default:
                return TC_MAPPING_CUSTOM;
        }
    }

    return mapping;
}

static bool tc_evaluateMappingRequest(void)
{
    CAN_digitalStatus_E requestIncStatus = CAN_DIGITALSTATUS_SNA;
    CAN_digitalStatus_E requestDecStatus = CAN_DIGITALSTATUS_SNA;
    const bool          requestInc       = (CANRX_get_signal(VEH, SWS_requestTcMappingInc, &requestIncStatus) == CANRX_MESSAGE_VALID) &&
                                           (requestIncStatus == CAN_DIGITALSTATUS_ON);
    const bool          requestDec       = (CANRX_get_signal(VEH, SWS_requestTcMappingDec, &requestDecStatus) == CANRX_MESSAGE_VALID) &&
                                           (requestDecStatus == CAN_DIGITALSTATUS_ON);
    const bool          requestIncRising = requestInc && !torque_data.tcMappingIncActive;
    const bool          requestDecRising = requestDec && !torque_data.tcMappingDecActive;
    const int16_t       requestSum       = requestIncRising - requestDecRising;

    torque_data.tcMappingIncActive = requestInc;
    torque_data.tcMappingDecActive = requestDec;

    if (requestSum != 0)
    {
        tc_setSelectedMapping(tc_stepMapping(tc_getSelectedMapping(), requestSum));
        drv_timer_start(&torque_data.paramTcChangeTimer, TC_PARAM_CHANGE_DELAY_MS);
        return true;
    }

    return false;
}

static bool evaluate_gear_change(float32_t accelerator_position, float32_t brake_position)
{
    bool                ret                       = false;
    CAN_digitalStatus_E gear_change_request       = CAN_DIGITALSTATUS_SNA;
    const bool          gear_change_was_requested = torque_data.gear_change_active;

    torque_data.gear_change_active = (CANRX_get_signal(VEH, SWS_requestReverse, &gear_change_request) != CANRX_MESSAGE_SNA) &&
                                     (gear_change_request == CAN_DIGITALSTATUS_ON);
    const bool      gear_change_rising = !gear_change_was_requested && torque_data.gear_change_active;
    const float32_t vehicleSpeed       = app_vehicleSpeed_getVehicleSpeed();
    if (gear_change_rising)
    {
#if FEATURE_IS_ENABLED(FEATURE_REVERSE)
        const bool resolverCalibrating = app_faultManager_getNetworkedFault_state(VEH, VCREAR_faults, FM_FAULT_VCREAR_MCCALIBRATINGRESOLVER);
        const bool ok_to_change        = (accelerator_position < PEDAL_APPLIED_THRESHOLD) &&
                                         (brake_position > PEDAL_APPLIED_THRESHOLD) &&
                                         (vehicleSpeed < VEHICLE_STOPPED_THRESHOLD) &&
                                         !resolverCalibrating;
        if (ok_to_change)
        {
            ret              = true;
            torque_data.gear = (torque_data.gear == GEAR_F) ? GEAR_R : GEAR_F;
        }
        else
#endif // if FEATURE_IS_ENABLED(FEATURE_REVERSE)
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
    bool                ret                            = false;
    CAN_digitalStatus_E race_mode_change_requested     = CAN_DIGITALSTATUS_SNA;
    const bool          race_mode_change_was_requested = torque_data.race_mode_change_active;

    torque_data.race_mode_change_active = (CANRX_get_signal(VEH, SWS_requestRaceMode, &race_mode_change_requested) != CANRX_MESSAGE_SNA) &&
                                          (race_mode_change_requested == CAN_DIGITALSTATUS_ON);
    const bool      race_mode_change_rising = !race_mode_change_was_requested && torque_data.race_mode_change_active;

    const float32_t vehicleSpeed            = app_vehicleSpeed_getVehicleSpeed();

    const bool      brakePedalPressed       = brake_position >= PEDAL_APPLIED_THRESHOLD;
    const bool      vehicleStationary       = vehicleSpeed < VEHICLE_STOPPED_THRESHOLD;

    if (race_mode_change_rising && brakePedalPressed && vehicleStationary)
    {
        ret                   = true;
        torque_data.race_mode = (torque_data.race_mode == RACEMODE_ENABLED) ? RACEMODE_PIT : RACEMODE_ENABLED;
    }

    return ret;
}

static float32_t evaluate_torque_max(void)
{
    nvm_tcPid_S         * pid                 = tc_getActivePid();
    float32_t           torque_request_max    = (float32_t)pid->maxTorqueNm;
    CAN_digitalStatus_E torque_change_request = CAN_DIGITALSTATUS_SNA;
    const bool          torque_inc_active     = (CANRX_get_signal(VEH, SWS_requestTorqueInc, &torque_change_request) != CANRX_MESSAGE_SNA) &&
                                                (torque_change_request == CAN_DIGITALSTATUS_ON);
    const bool          torque_dec_active     = (CANRX_get_signal(VEH, SWS_requestTorqueDec, &torque_change_request) != CANRX_MESSAGE_SNA) &&
                                                (torque_change_request == CAN_DIGITALSTATUS_ON);

    if (torque_inc_active ^ torque_dec_active)
    {
        const drv_timer_state_E timer_state = drv_timer_getState(&torque_data.torque_change_timer);
        if (timer_state == DRV_TIMER_STOPPED)
        {
            drv_timer_start(&torque_data.torque_change_timer, TORQUE_CHANGE_DELAY);
            torque_request_max = torque_inc_active ? torque_request_max + 1 : torque_request_max - 1;
            torque_request_max = SATURATE(MIN_TORQUE_RANGE, torque_request_max, ABSOLUTE_MAX_TORQUE);
            pid->maxTorqueNm   = (uint16_t)torque_request_max;
            tc_requestCustomPidWriteAfterActivity();
        }
        else if (timer_state == DRV_TIMER_EXPIRED)
        {
            drv_timer_stop(&torque_data.torque_change_timer);
        }
    }
    else
    {
        drv_timer_stop(&torque_data.torque_change_timer);
    }

    torque_data.torque_request_max = torque_request_max;
    return torque_request_max;
}

static void evaluate_launch_control(float32_t accelerator_position, float32_t brake_position, bool bppc_ok)
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
            CAN_digitalStatus_E launch_control_requested          = CAN_DIGITALSTATUS_SNA;
            const bool          launch_control_request_was_active = torque_data.torque_control_request_active;
            torque_data.torque_control_request_active = (CANRX_get_signal(VEH, SWS_requestLaunchControl, &launch_control_requested) != CANRX_MESSAGE_SNA) &&
                                                        (launch_control_requested == CAN_DIGITALSTATUS_ON);
            const bool          launch_control_request_rising     = !launch_control_request_was_active && torque_data.torque_control_request_active;

            if (drv_timer_getState(&torque_data.launch_control_timer) == DRV_TIMER_EXPIRED)
            {
                torque_data.launchControlState = LC_STATE_INACTIVE;
                drv_timer_stop(&torque_data.launch_control_timer);
            }
            else if (launch_control_request_rising)
            {
                if ((brake_position > LC_BRAKE_THRESHOLD) &&
                    (accelerator_position < LC_THROTTLE_START_CUTOFF) &&
                    bppc_ok
                    )
                {
                    torque_data.launchControlState = LC_STATE_HOLDING;
                    launch_control_75m_reset();
                    drv_timer_stop(&torque_data.launch_control_timer);
                }
                else
                {
                    launchRejected                 = true;
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
            else if (!bppc_ok)
            {
                launchRejected                 = true;
                torque_data.launchControlState = LC_STATE_REJECTED;
                drv_timer_start(&torque_data.launch_control_timer, LC_REJECTED_MS);
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
            else if (!bppc_ok)
            {
                launchRejected                 = true;
                torque_data.launchControlState = LC_STATE_REJECTED;
                drv_timer_start(&torque_data.launch_control_timer, LC_REJECTED_MS);
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
            else if (!bppc_ok)
            {
                launchRejected                 = true;
                torque_data.launchControlState = LC_STATE_REJECTED;
                drv_timer_start(&torque_data.launch_control_timer, LC_REJECTED_MS);
            }
            else if (brake_position < LC_BRAKE_THRESHOLD_LAUNCH)
            {
                torque_data.launchControlState = LC_STATE_LAUNCH;
                launch_control_75m_start();
            }
            break;

        case LC_STATE_LAUNCH:
            if (!bppc_ok)
            {
                launchRejected                 = true;
                torque_data.launchControlState = LC_STATE_REJECTED;
                drv_timer_start(&torque_data.launch_control_timer, LC_REJECTED_MS);
            }
            else if ((accelerator_position < LC_THROTTLE_START_CUTOFF) ||
                     (brake_position > LC_BRAKE_THRESHOLD)
                     )
            {
                torque_data.launchControlState = LC_STATE_INACTIVE;
            }
            break;

        default:
            break;
    }
#else // if FEATURE_IS_ENABLED(FEATURE_LAUNCH_CONTROL)
    UNUSED(accelerator_position);
    UNUSED(brake_position);
    UNUSED(bppc_ok);
#endif // if FEATURE_IS_ENABLED(FEATURE_LAUNCH_CONTROL)

    if ((torque_data.launchControlState != LC_STATE_INACTIVE) &&
        (torque_data.launchControlState != LC_STATE_REJECTED) &&
        ((torque_data.race_mode != RACEMODE_ENABLED) ||
         (torque_data.gear != GEAR_F) ||
         (torque_data.state != TORQUE_ACTIVE))
        )
    {
        launchRejected                 = true;
        torque_data.launchControlState = LC_STATE_REJECTED;
        drv_timer_start(&torque_data.launch_control_timer, LC_REJECTED_MS);
    }

    app_faultManager_setFaultState(FM_FAULT_VCFRONT_LAUNCHREJECTED, launchRejected);
    launch_control_75m_update();
}

static float32_t calc_traction_control_reduction(float32_t target_slip, float32_t actual_slip, float32_t dt)
{
    const nvm_tcPid_S* pid = tc_getActivePidConst();
    const float32_t  kLeak = 1.0f / TC_PID_CONV_THOU_F32(pid->tLeakMs);

    lib_pid_util_ileak(&torque_data.tractionControlPID, kLeak, dt);
    torque_data.tractionControlPID.kp = TC_PID_CONV_THOU_F32(pid->thousandthKp);
    torque_data.tractionControlPID.ki = TC_PID_CONV_THOU_F32(pid->thousandthKi);
    torque_data.tractionControlPID.kd = TC_PID_CONV_THOU_F32(-pid->thousandthKd);
    lib_pi_typeb_calc(&torque_data.tractionControlPID, target_slip, actual_slip, dt);
    lib_pid_util_ilim(&torque_data.tractionControlPID, TC_MIN, TC_PID_CONV_PERCENT_F32(pid->percentILim));
    lib_pid_util_lpf_dTerm(&torque_data.tractionControlPID, dt);
    lib_pid_typeb_sum(&torque_data.tractionControlPID, TC_MIN, TC_PID_CONV_PERCENT_F32(pid->percentMaxTcLimit));

    return torque_data.tractionControlPID.y;
}

static float32_t evaluate_traction_control(void)
{
    const uint32_t  timestamp = HW_TIM_getTimeMS();
    const float32_t dt        = ((float32_t)(timestamp - torque_data.lastTimeampMS)) / 1000.0f;

    torque_data.lastTimeampMS = timestamp;

    const float32_t               vehicleSpeed               = app_vehicleSpeed_getVehicleSpeed();
    const float32_t               slip                       = app_vehicleSpeed_getAxleSlip(AXLE_REAR);
    float32_t                     multiplier                 = 0.0f;

    torque_tractionControlState_E nextState                  = TC_STATE_ERROR;

#if FEATURE_IS_ENABLED(FEATURE_TRACTION_CONTROL)
    CAN_digitalStatus_E           traction_control_requested = CAN_DIGITALSTATUS_SNA;
    bool                          requested                  = (CANRX_get_signal(VEH, SWS_requestTractionControl, &traction_control_requested) != CANRX_MESSAGE_SNA) &&
                                                               (traction_control_requested == CAN_DIGITALSTATUS_ON);
    const bool                    tcAllowed                  = (torque_data.gear == GEAR_F) &&
                                                               (torque_data.race_mode == RACEMODE_ENABLED);
    if (requested)
    {
        nextState = tcAllowed ? TC_STATE_ACTIVE : TC_STATE_LOCKOUT;
    }
    else
#endif // if FEATURE_IS_ENABLED(FEATURE_TRACTION_CONTROL)
    {
        nextState = TC_STATE_INACTIVE;
    }

    torque_data.tractionControlState = nextState;

    if ((torque_data.tractionControlState == TC_STATE_ACTIVE) &&
        (vehicleSpeed > TC_VEHICLESPEED_THRESHOLD_MPS))
    {
        multiplier = calc_traction_control_reduction(torque_data.slip_request, slip, dt);
    }
    else
    {
        const nvm_tcPid_S* pid = tc_getActivePidConst();
        lib_pid_init(&torque_data.tractionControlPID, 0.0f, 0.0f,
                     TC_PID_CONV_THOU_F32(pid->thousandthKp),
                     TC_PID_CONV_THOU_F32(pid->thousandthKi),
                     TC_PID_CONV_THOU_F32(-pid->thousandthKd));
    }

    torque_data.slipRear = slip;
    return multiplier;
}

static void evaluateRegenEnabled(float32_t accelPosition, float32_t brakePosition)
{
    bool                regenAllowed = false;

#if FEATURE_IS_ENABLED(FEATURE_REGEN)
    CAN_digitalStatus_E regenEnabled = CAN_DIGITALSTATUS_SNA;
    bool                requested    = (CANRX_get_signal(VEH, SWS_requestRegenEnabled, &regenEnabled) != CANRX_MESSAGE_SNA) &&
                                       (regenEnabled == CAN_DIGITALSTATUS_ON);
    const float32_t     vehicleSpeed = app_vehicleSpeed_getVehicleSpeed();

    torque_data.regenEnabled = requested && (torque_data.gear == GEAR_F) &&
                               (accelPosition < REGEN_ACCEL_CUTOFF);
    regenAllowed             = torque_data.regenEnabled && (vehicleSpeed > REGEN_SPEED_CUTOFF_MPS);
#else
    torque_data.regenEnabled = false;
#endif

    if (regenAllowed)
    {
        if (torque_data.isRegenerating)
        {
            if (brakePosition < REGEN_BRAKE_STOP_THRESH)
            {
                torque_data.isRegenerating = false;
            }
        }
        else
        {
            if (brakePosition > REGEN_BRAKE_START_THRESH)
            {
                torque_data.isRegenerating = true;
            }
        }
    }
    else
    {
        torque_data.isRegenerating = false;
    }
}

static void evaluate_slip_request(void)
{
    float32_t           slip_request        = torque_data.slip_request;
    CAN_digitalStatus_E slip_change_request = CAN_DIGITALSTATUS_SNA;
    const bool          slip_inc_active     = (CANRX_get_signal(VEH, SWS_requestSlipInc, &slip_change_request) != CANRX_MESSAGE_SNA) &&
                                              (slip_change_request == CAN_DIGITALSTATUS_ON);
    const bool          slip_dec_active     = (CANRX_get_signal(VEH, SWS_requestSlipDec, &slip_change_request) != CANRX_MESSAGE_SNA) &&
                                              (slip_change_request == CAN_DIGITALSTATUS_ON);

    if (slip_inc_active ^ slip_dec_active)
    {
        const drv_timer_state_E timer_state = drv_timer_getState(&torque_data.slip_change_timer);
        if (timer_state == DRV_TIMER_STOPPED)
        {
            drv_timer_start(&torque_data.slip_change_timer, SLIP_CHANGE_DELAY);

            slip_request = slip_inc_active
                ? (slip_request + SLIP_TARGET_STEP)
                : (slip_request - SLIP_TARGET_STEP);

            slip_request             = SATURATE(ABSOLUTE_MIN_SLIP, slip_request, ABSOLUTE_MAX_SLIP);
            torque_data.slip_request = slip_request;
        }
        else if (timer_state == DRV_TIMER_EXPIRED)
        {
            drv_timer_stop(&torque_data.slip_change_timer);
        }
    }
    else
    {
        drv_timer_stop(&torque_data.slip_change_timer);
    }
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
    float32_t           torque_request        = torque_data.torquePreload;
    CAN_digitalStatus_E torque_change_request = CAN_DIGITALSTATUS_SNA;
    const bool          torque_inc_active     = (CANRX_get_signal(VEH, SWS_requestPreloadTorqueInc, &torque_change_request) != CANRX_MESSAGE_SNA) &&
                                                (torque_change_request == CAN_DIGITALSTATUS_ON);
    const bool          torque_dec_active     = (CANRX_get_signal(VEH, SWS_requestPreloadTorqueDec, &torque_change_request) != CANRX_MESSAGE_SNA) &&
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

static void tcEvaluateParams(void)
{
    const drv_timer_state_E timerChangeState  = drv_timer_getState(&torque_data.paramTcChangeTimer);
    const drv_timer_state_E timerSaveState    = drv_timer_getState(&torque_data.paramTcSaveTimer);
    bool                    paramValueChanged = false;
    const bool              mappingChanged    = tc_evaluateMappingRequest();
    nvm_tcPid_S             * activePid       = tc_getActivePid();
    const bool              activePidSaved    = tc_isCustomMappingSelected();

    for (uint8_t i = 0U; i < PARAMSTATE_COUNT; i++)
    {
        CAN_digitalStatus_E request         = CAN_DIGITALSTATUS_SNA;
        const bool          wasRequested    = FLAG_get(torque_data.tcParamsWasRequested, i);
        const bool          isRequested     = (paramRequestStateCanSignal[i](&request) == CANRX_MESSAGE_VALID) &&
                                              (request == CAN_DIGITALSTATUS_ON);
        const bool          isRisingRequest = isRequested && !wasRequested;
        const bool          isParamSet      = FLAG_get(tcParamState_data.params, i);

        FLAG_assign(tcParamState_data.params, i, isRisingRequest ^ isParamSet);

        if (isRisingRequest)
        {
            lib_nvm_requestWrite(NVM_ENTRYID_TC_PARAMSTATE);
        }

        FLAG_assign(torque_data.tcParamsWasRequested, i, isRequested);
    }

    for (uint8_t i = 0U; i < PARAMVALUE_COUNT; i++)
    {
        CAN_digitalStatus_E request    = CAN_DIGITALSTATUS_SNA;
        const bool          requestInc = (paramValues[i].requestInc(&request) == CANRX_MESSAGE_VALID) &&
                                         (request == CAN_DIGITALSTATUS_ON);
        const bool          requestDec = (paramValues[i].requestDec(&request) == CANRX_MESSAGE_VALID) &&
                                         (request == CAN_DIGITALSTATUS_ON);
        const int16_t       requestSum = requestInc - requestDec;

        paramValueChanged |= requestSum != 0U;

        if (timerChangeState == DRV_TIMER_RUNNING)
        {
            continue;
        }

        switch (i)
        {
            case PARAMVALUE_TC_KP:
                if (((activePid->thousandthKp > 0U) && (activePid->thousandthKp < 65535U)) ||
                    ((activePid->thousandthKp == 0U) && (requestSum > 0)) ||
                    ((activePid->thousandthKp == 65535U) && (requestSum < 0))
                    )
                {
                    activePid->thousandthKp = (uint16_t)(activePid->thousandthKp + requestSum);
                }
                break;

            case PARAMVALUE_TC_KI:
                if (((activePid->thousandthKi > 0U) && (activePid->thousandthKi < 65535U)) ||
                    ((activePid->thousandthKi == 0U) && (requestSum > 0)) ||
                    ((activePid->thousandthKi == 65535U) && (requestSum < 0))
                    )
                {
                    activePid->thousandthKi = (uint16_t)(activePid->thousandthKi + requestSum);
                }
                break;

            case PARAMVALUE_TC_KD:
                if (((activePid->thousandthKd > 0U) && (activePid->thousandthKd < 65535U)) ||
                    ((activePid->thousandthKd == 0U) && (requestSum > 0)) ||
                    ((activePid->thousandthKd == 65535U) && (requestSum < 0))
                    )
                {
                    activePid->thousandthKd = (uint16_t)(activePid->thousandthKd + requestSum);
                }
                break;

            case PARAMVALUE_TC_MAX_LIM:
                if (((activePid->percentMaxTcLimit > 0U) && (activePid->percentMaxTcLimit < 100U)) ||
                    ((activePid->percentMaxTcLimit == 0U) && (requestSum > 0)) ||
                    ((activePid->percentMaxTcLimit == 100U) && (requestSum < 0))
                    )
                {
                    activePid->percentMaxTcLimit = (uint8_t)(activePid->percentMaxTcLimit + requestSum);
                }
                break;

            case PARAMVALUE_TC_ILIM:
                if (((activePid->percentILim > 0U) && (activePid->percentILim < 100U)) ||
                    ((activePid->percentILim == 0U) && (requestSum > 0)) ||
                    ((activePid->percentILim == 100U) && (requestSum < 0))
                    )
                {
                    activePid->percentILim = (uint8_t)(activePid->percentILim + requestSum);
                }
                break;

            case PARAMVALUE_TC_TLEAK_MS:
                if (((activePid->tLeakMs > 1U) && (activePid->tLeakMs < 65535U)) ||
                    ((activePid->tLeakMs == 1U) && (requestSum > 0)) ||
                    ((activePid->tLeakMs == 65535U) && (requestSum < 0))
                    )
                {
                    activePid->tLeakMs = (uint16_t)(activePid->tLeakMs + requestSum);
                }
                break;
        }
    }

    if (paramValueChanged)
    {
        if (timerChangeState != DRV_TIMER_RUNNING)
        {
            drv_timer_start(&torque_data.paramTcChangeTimer, TC_PARAM_CHANGE_DELAY_MS);
        }

        if (activePidSaved)
        {
            tc_requestCustomPidWriteAfterActivity();
        }
    }
    else
    {
        if (timerSaveState == DRV_TIMER_EXPIRED)
        {
            drv_timer_stop(&torque_data.paramTcSaveTimer);
            drv_timer_stop(&torque_data.paramTcChangeTimer);

            if (torque_data.customTcPidWritePending)
            {
                torque_data.customTcPidWritePending = false;
                lib_nvm_requestWrite(NVM_ENTRYID_TC_PID);
            }
        }

        if (!mappingChanged)
        {
            drv_timer_stop(&torque_data.paramTcChangeTimer);
        }
    }
}
static float32_t evaluate_regenTorque(void)
{
    float32_t  decel;
    const bool accel_valid = (CANRX_get_signal(VEH, VCPDU_lon, &decel) == CANRX_MESSAGE_VALID);
    float32_t  brakePressure_rear;
    const bool brake_valid = (CANRX_get_signal(VEH, VCREAR_brakePressure, &brakePressure_rear) == CANRX_MESSAGE_VALID);

    if (accel_valid && brake_valid)
    {
        float32_t weightTranfer           = -(decel * CG_HEIGHT * CAR_MASS) / WHEEL_BASE;
        float32_t dynamic_rearWeight      = CAR_WEIGHT / 2 - weightTranfer; // assuming 50/50 weight front/rear
        float32_t rearWeight_percentage   = dynamic_rearWeight / (CAR_WEIGHT);
        float32_t total_brakeForce        = decel * CAR_MASS;
        float32_t required_rearAxleForce  = total_brakeForce * rearWeight_percentage;
        float32_t required_rearAxleTorque = required_rearAxleForce * (WHEEL_DIAMETER / 2);
        brakePressure_rear = brakePressure_rear * 6895;    // converting  PSI to PA
        float32_t brake_torque            = brakePressure_rear * REAR_CALIPER_AREA * EFFECTIVE_ROTOR_RADIUS;
        float32_t regen_torque            = required_rearAxleTorque - brake_torque;
        regen_torque       = regen_torque / GEAR_RATIO;
        return regen_torque;
    }
    else
    {
        return 0;
    }
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
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

float32_t torque_getSlipTarget(void)
{
    return torque_data.slip_request;
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

float32_t torque_getVdMaxTorqueRequest(void)
{
    return torque_data.maxVdTorque;
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

float32_t torque_getLaunchControl75mTime(void)
{
    return torque_data.launch75mTime;
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

bool tc_isParamEnabled(tc_paramState_E param)
{
    return FLAG_get(tcParamState_data.params, param);
}

CAN_tcMapping_E tc_getMappingCAN(void)
{
    CAN_tcMapping_E mapping = CAN_TCMAPPING_SNA;

    switch (tc_getSelectedMapping())
    {
        case TC_MAPPING_CUSTOM:
            mapping = CAN_TCMAPPING_CUSTOM;
            break;

        case TC_MAPPING_100NM:
            mapping = CAN_TCMAPPING_MAP_100NM;
            break;

        case TC_MAPPING_150NM:
            mapping = CAN_TCMAPPING_MAP_150NM;
            break;

        default:
            break;
    }

    return mapping;
}

float32_t tc_getParamPidMax(void)
{
    return TC_PID_CONV_PERCENT_F32(tc_getActivePidConst()->percentMaxTcLimit);
}

float32_t tc_getParamILim(void)
{
    return TC_PID_CONV_PERCENT_F32(tc_getActivePidConst()->percentILim);
}

float32_t tc_getParamKp(void)
{
    return TC_PID_CONV_THOU_F32(tc_getActivePidConst()->thousandthKp);
}

float32_t tc_getParamKi(void)
{
    return TC_PID_CONV_THOU_F32(tc_getActivePidConst()->thousandthKi);
}

float32_t tc_getParamKd(void)
{
    return TC_PID_CONV_THOU_F32(tc_getActivePidConst()->thousandthKd);
}

float32_t tc_getParamTLeak(void)
{
    return TC_PID_CONV_THOU_F32(tc_getActivePidConst()->tLeakMs);
}

static void torque_init(void)
{
    memset(&torque_data, 0x00U, sizeof(torque_data));

    drv_timer_init(&torque_data.torque_change_timer);
    drv_timer_init(&torque_data.launch_control_timer);
    drv_timer_init(&torque_data.launch_75m_timer);
    drv_timer_init(&torque_data.preloadChangeTimer);
    drv_timer_init(&torque_data.slip_change_timer);
    drv_timer_init(&torque_data.paramTcSaveTimer);
    drv_timer_init(&torque_data.paramTcChangeTimer);

    torque_data.tcPid100Nm = tcPid100NmDefault;
    torque_data.tcPid150Nm = tcPid150NmDefault;

    if ((tcPid_data.maxTorqueNm < MIN_TORQUE_RANGE) ||
        (tcPid_data.maxTorqueNm > ABSOLUTE_MAX_TORQUE)
        )
    {
        tcPid_data.maxTorqueNm = TC_130NM_TORQUE;
        lib_nvm_requestWrite(NVM_ENTRYID_TC_PID);
    }

    if (tcParamState_data.selectedTcMapping >= TC_MAPPING_COUNT)
    {
        tcParamState_data.selectedTcMapping = TC_MAPPING_CUSTOM;
        lib_nvm_requestWrite(NVM_ENTRYID_TC_PARAMSTATE);
    }

    torque_data.state                         = TORQUE_INACTIVE;
    torque_data.torque_request_max            = (float32_t)tc_getActivePidConst()->maxTorqueNm;
    torque_data.gear                          = GEAR_F;
    torque_data.launchControlState            = LC_STATE_INACTIVE;
    torque_data.tractionControlState          = TC_STATE_INACTIVE;

    torque_data.slip_request                  = TC_TARGET_SLIP;
    torque_data.torqueRateLimit.y_n           = 0.0f;
    torque_data.torqueRateLimit.maxStepDelta  = MAX_TORQUE_NM_PER_S / 100;
    torque_data.launchRateLimit.y_n           = 0.0f;
    torque_data.launchRateLimit.maxStepDelta  = MAX_LAUNCH_NM_PER_S / 100;
    torque_data.preloadRateLimit.y_n          = 0.0f;
    torque_data.preloadRateLimit.maxStepDelta = PRELOAD_NM_PER_S / 100;

    torque_data.torquePreload                 = LC_PRELOAD_TORQUE_INIT;

    const nvm_tcPid_S* pid = tc_getActivePidConst();
    lib_pid_init(&torque_data.tractionControlPID, 0.0f, 0.0f,
                 TC_PID_CONV_THOU_F32(pid->thousandthKp),
                 TC_PID_CONV_THOU_F32(pid->thousandthKi),
                 TC_PID_CONV_THOU_F32(-pid->thousandthKd));
    lib_pid_util_lpf_dTermSetCutoff(&torque_data.tractionControlPID, TC_DTERM_LPF_CUTOFF_FREQ);
}

static void torque_periodic_100Hz(void)
{
    tcEvaluateParams();
    const float32_t    accelerator_position = apps_getPedalPosition();
    const float32_t    brake_position       = bppc_getPedalPosition();
    const bppc_state_E bppc_ok              = bppc_getState() == BPPC_OK;
    torque_data.state = (app_vehicleState_getState() == VEHICLESTATE_TS_RUN) ? TORQUE_ACTIVE : TORQUE_INACTIVE;
    evaluate_sleepable(accelerator_position, brake_position);

    const bool gear_change = evaluate_gear_change(accelerator_position, brake_position);
    const bool mode_change = evaluate_mode_change(brake_position);
    evaluate_slip_request();
    evaluate_preload_torque();

    evaluate_launch_control(accelerator_position, brake_position, bppc_ok);
    evaluateRegenEnabled(accelerator_position, brake_position);
    torque_data.torqueReduction = evaluate_traction_control();

    float32_t torque_request_max = evaluate_torque_max();
    torque_request_max          = (torque_data.race_mode != RACEMODE_ENABLED) ? DEFAULT_TORQUE_PITS : torque_request_max;
    torque_request_max          = (torque_data.gear != GEAR_F) ? DEFAULT_TORQUE_LIMIT_REVERSE : torque_request_max;

    if (gear_change || mode_change || !bppc_ok)
    {
        torque_data.torqueRateLimit.y_n = 0.0f;
    }

    float32_t       torque      = (bppc_ok) ? accelerator_position * torque_request_max : 0.0f;
    const float32_t maxVdTorque = ((vd_getMaxLonTireForce(WHEEL_RL) + vd_getMaxLonTireForce(WHEEL_RR)) * TIRE_RADIUS_M) / GEAR_RATIO;
    torque_data.maxVdTorque = maxVdTorque;
#if FEATURE_IS_ENABLED(FEATURE_LIMIT_LON_TIRE_ACCEL)
    if (FLAG_get(tcParamState_data.params, PARAMSTATE_TC_TIRE_MODEL_LIMIT))
    {
        torque = SATURATE(-maxVdTorque, torque, maxVdTorque);
    }
#endif
#if FEATURE_IS_ENABLED(FEATURE_REGEN)
    torque_data.regenTorque       = evaluate_regenTorque();
#endif
#if !FEATURE_IS_ENABLED(FEATURE_REGEN)
    torque_data.regenTorque       = 0;
#endif
    torque_data.torqueDriverInput = torque;
    torque_data.torqueCorrection  = torque_data.torqueReduction * torque;
    const float32_t torqueOutput = torque - torque_data.torqueCorrection;

    if ((torque_data.launchControlState == LC_STATE_HOLDING) ||
        (torque_data.launchControlState == LC_STATE_SETTLING)
        )
    {
        torque                           = 0.0f;
        torque_data.torqueRateLimit.y_n  = 0.0f;
        torque_data.launchRateLimit.y_n  = 0.0f;
        torque_data.preloadRateLimit.y_n = 0.0f;
    }
    else if (torque_data.launchControlState == LC_STATE_PRELOAD)
    {
        torque                          = lib_rateLimit_linear_update(&torque_data.preloadRateLimit, torque_data.torquePreload);
        torque_data.launchRateLimit.y_n = torque;
    }
    else if (torque_data.launchControlState == LC_STATE_LAUNCH)
    {
        torque                          = lib_rateLimit_linear_update(&torque_data.launchRateLimit, torqueOutput);
        torque_data.torqueRateLimit.y_n = torque;
    }
    else
    {
        torque = !torque_data.isRegenerating ? torqueOutput : -torque_data.regenTorque;
        torque = lib_rateLimit_linear_update(&torque_data.torqueRateLimit, torque);
    }

    const float32_t minTorque = (torque_data.gear == GEAR_F) ? -REGEN_MAX_TORQUE_N : ABSOLUTE_MIN_TORQUE;
    torque_data.torque = SATURATE(minTorque, torque, ABSOLUTE_MAX_TORQUE);
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S torque_desc = {
    .moduleInit        = &torque_init,
    .periodic100Hz_CLK = &torque_periodic_100Hz,
};
