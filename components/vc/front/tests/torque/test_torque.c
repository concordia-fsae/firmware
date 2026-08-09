#include "torque.h"
#include "unity.h"

#include "app_vehicleState.h"
#include "apps.h"
#include "bppc.h"
#include "ModuleDesc.h"
#include "vd.h"

#include <math.h>
#include <string.h>

static uint32_t                 stubTimeMs;
static float32_t                stubAcceleratorPosition;
static float32_t                stubBrakePosition;
static bppc_state_E             stubBppcState;
static app_vehicleState_state_E stubVehicleState;
static float32_t                stubVehicleSpeed;
static float32_t                stubRearAxleSlip;
static CAN_digitalStatus_E      stubDigitalSignals[32U];
static bool                     stubFaults[FM_FAULT_COUNT];
static CANRX_rawMessage_S       stubRawMessage;

extern const ModuleDesc_S       torque_desc;

nvm_tcParamState_S              tcParamState_data;
nvm_tcPid_S                     tcPid_data;

static void setDefaultCustomPid(void)
{
    tcPid_data.percentMaxTcLimit = 70U;
    tcPid_data.percentILim       = 0U;
    tcPid_data.thousandthKp      = 591U;
    tcPid_data.thousandthKi      = 0U;
    tcPid_data.thousandthKd      = 70U;
    tcPid_data.tLeakMs           = 500U;
    tcPid_data.maxTorqueNm       = 130U;
}

static void run100Hz(void)
{
    stubTimeMs += 10U;
    torque_desc.periodic100Hz_CLK();
}

static void enterRaceMode(void)
{
    stubBrakePosition                       = 0.2f;
    stubVehicleSpeed                        = 0.0f;
    stubDigitalSignals[SWS_requestRaceMode] = CAN_DIGITALSTATUS_ON;
    run100Hz();

    stubDigitalSignals[SWS_requestRaceMode] = CAN_DIGITALSTATUS_OFF;
    stubBrakePosition                       = 0.0f;
    run100Hz();

    TEST_ASSERT_EQUAL(RACEMODE_ENABLED, torque_getRaceMode());
}

static void enableTractionControl(void)
{
    enterRaceMode();
    stubDigitalSignals[SWS_requestTractionControl] = CAN_DIGITALSTATUS_ON;
}

void setUp(void)
{
    stubTimeMs              = 0U;
    stubAcceleratorPosition = 1.0f;
    stubBrakePosition       = 0.0f;
    stubBppcState           = BPPC_OK;
    stubVehicleState        = VEHICLESTATE_TS_RUN;
    stubVehicleSpeed        = 10.0f;
    stubRearAxleSlip        = 0.0f;
    memset(stubDigitalSignals, 0x00, sizeof(stubDigitalSignals));
    memset(stubFaults,         0x00, sizeof(stubFaults));
    memset(&stubRawMessage,    0x00, sizeof(stubRawMessage));
    memset(&tcParamState_data, 0x00, sizeof(tcParamState_data));
    memset(&tcPid_data,        0x00, sizeof(tcPid_data));
    setDefaultCustomPid();

    torque_desc.moduleInit();
}

void tearDown(void)
{
}

CANRX_MESSAGE_health_E CANRX_get_signal(CAN_bus_E bus, CAN_signal_E signal, void* value)
{
    (void)bus;

    switch (signal)
    {
        case VCPDU_lon:
        case VCREAR_brakePressure:
            *((float32_t*)value) = 0.0f;
            return CANRX_MESSAGE_VALID;

        default:
            *((CAN_digitalStatus_E*)value) = stubDigitalSignals[signal];
            return CANRX_MESSAGE_VALID;
    }
}

CANRX_MESSAGE_health_E CANRX_get_signal_digitalSna(CAN_digitalStatus_E* status)
{
    *status = CAN_DIGITALSTATUS_SNA;
    return CANRX_MESSAGE_SNA;
}

CANRX_rawMessage_S* CANRX_get_rawMessage(CAN_bus_E bus, CAN_message_E message)
{
    (void)bus;
    (void)message;
    return &stubRawMessage;
}

uint32_t HW_TIM_getTimeMS(void)
{
    return stubTimeMs;
}

bool lib_nvm_requestWrite(lib_nvm_entryId_E entryId)
{
    (void)entryId;
    return true;
}

void app_faultManager_setFaultState(FM_fault_E fault, bool faulted)
{
    stubFaults[fault] = faulted;
}

bool app_faultManager_getFaultState(FM_fault_E fault)
{
    return stubFaults[fault];
}

float32_t app_vehicleSpeed_getVehicleSpeed(void)
{
    return stubVehicleSpeed;
}

float32_t app_vehicleSpeed_getOdometer(void)
{
    return 0.0f;
}

float32_t app_vehicleSpeed_getAxleSlip(axle_E axle)
{
    (void)axle;
    return stubRearAxleSlip;
}

void app_vehicleState_delaySleep(uint32_t ms)
{
    (void)ms;
}

app_vehicleState_state_E app_vehicleState_getState(void)
{
    return stubVehicleState;
}

float32_t apps_getPedalPosition(void)
{
    return stubAcceleratorPosition;
}

apps_state_E apps_getState(void)
{
    return APPS_OK;
}

float32_t bppc_getPedalPosition(void)
{
    return stubBrakePosition;
}

bppc_state_E bppc_getState(void)
{
    return stubBppcState;
}

float32_t vd_getMaxLonTireForce(wheel_E wheel)
{
    (void)wheel;
    return 10000.0f;
}

void test_traction_control_inactive_when_not_requested(void)
{
    enterRaceMode();

    run100Hz();

    TEST_ASSERT_EQUAL(TC_STATE_INACTIVE, torque_getTractionControlState());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, torque_getTorqueReduction());
}

void test_traction_control_reduction_caps_to_configured_limit(void)
{
    enableTractionControl();
    stubRearAxleSlip = 100.0f;

    run100Hz();

    TEST_ASSERT_EQUAL(TC_STATE_ACTIVE, torque_getTractionControlState());
    TEST_ASSERT_TRUE(isfinite(torque_getTorqueReduction()));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.70f, torque_getTorqueReduction());
}

void test_nan_slip_is_reported_as_zero_and_does_not_reduce_torque(void)
{
    enableTractionControl();
    stubRearAxleSlip = NAN;

    run100Hz();

    TEST_ASSERT_TRUE(isfinite(torque_getSlipRaw()));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, torque_getSlipRaw());
    TEST_ASSERT_TRUE(isfinite(torque_getTorqueReduction()));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, torque_getTorqueReduction());
}

void test_positive_infinite_slip_is_reported_as_zero_and_does_not_saturate(void)
{
    enableTractionControl();
    stubRearAxleSlip = INFINITY;

    run100Hz();

    TEST_ASSERT_TRUE(isfinite(torque_getSlipRaw()));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, torque_getSlipRaw());
    TEST_ASSERT_TRUE(isfinite(torque_getTorqueReduction()));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, torque_getTorqueReduction());
}

void test_negative_infinite_slip_is_reported_as_zero_and_does_not_saturate(void)
{
    enableTractionControl();
    stubRearAxleSlip = -INFINITY;

    run100Hz();

    TEST_ASSERT_TRUE(isfinite(torque_getSlipRaw()));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, torque_getSlipRaw());
    TEST_ASSERT_TRUE(isfinite(torque_getTorqueReduction()));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, torque_getTorqueReduction());
}

void test_corrupt_pid_max_limit_cannot_reduce_more_than_all_torque(void)
{
    tcPid_data.percentMaxTcLimit = 250U;
    tcPid_data.thousandthKp      = 10000U;
    enableTractionControl();
    stubRearAxleSlip             = 100.0f;

    run100Hz();

    TEST_ASSERT_TRUE(isfinite(torque_getTorqueReduction()));
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(1.0f, torque_getTorqueReduction());
}

void test_zero_integral_leak_time_does_not_poison_reduction(void)
{
    tcPid_data.percentILim  = 100U;
    tcPid_data.thousandthKi = 1000U;
    tcPid_data.tLeakMs      = 0U;
    enableTractionControl();
    stubRearAxleSlip        = 1.0f;

    run100Hz();
    run100Hz();

    TEST_ASSERT_TRUE(isfinite(torque_getSlipErrorI()));
    TEST_ASSERT_TRUE(isfinite(torque_getTorqueReduction()));
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(1.0f, torque_getTorqueReduction());
}

void test_corrupt_integral_limit_cannot_reduce_more_than_all_torque(void)
{
    tcPid_data.percentMaxTcLimit = 250U;
    tcPid_data.percentILim       = 250U;
    tcPid_data.thousandthKi      = 10000U;
    enableTractionControl();
    stubRearAxleSlip             = 100.0f;

    run100Hz();
    run100Hz();

    TEST_ASSERT_TRUE(isfinite(torque_getSlipErrorI()));
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(1.0f, torque_getSlipErrorI());
    TEST_ASSERT_TRUE(isfinite(torque_getTorqueReduction()));
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(1.0f, torque_getTorqueReduction());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_traction_control_inactive_when_not_requested);
    RUN_TEST(test_traction_control_reduction_caps_to_configured_limit);
    RUN_TEST(test_nan_slip_is_reported_as_zero_and_does_not_reduce_torque);
    RUN_TEST(test_positive_infinite_slip_is_reported_as_zero_and_does_not_saturate);
    RUN_TEST(test_negative_infinite_slip_is_reported_as_zero_and_does_not_saturate);
    RUN_TEST(test_corrupt_pid_max_limit_cannot_reduce_more_than_all_torque);
    RUN_TEST(test_zero_integral_leak_time_does_not_poison_reduction);
    RUN_TEST(test_corrupt_integral_limit_cannot_reduce_more_than_all_torque);
    return UNITY_END();
}
