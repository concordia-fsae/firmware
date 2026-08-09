#include "app_vehicleSpeed.h"
#include "unity.h"

#include <math.h>
#include <stddef.h>

#define WHEEL_CIRCUMFERENCE_M    1.25679f

static uint16_t               stubWheelRpm[WHEEL_CNT];
static CANRX_MESSAGE_health_E stubWheelHealth[WHEEL_CNT];
static float32_t              stubTimerHz[HW_TIM_CHANNEL_WS_CNT];
static float32_t              stubVehicleSpeed;
static CANRX_MESSAGE_health_E stubVehicleSpeedHealth;

static CANRX_MESSAGE_health_E getWheelFlRpm(uint16_t* rpm)
{
    *rpm = stubWheelRpm[WHEEL_FL];
    return stubWheelHealth[WHEEL_FL];
}

static CANRX_MESSAGE_health_E getWheelFrRpm(uint16_t* rpm)
{
    *rpm = stubWheelRpm[WHEEL_FR];
    return stubWheelHealth[WHEEL_FR];
}

static CANRX_MESSAGE_health_E getWheelRlRpm(uint16_t* rpm)
{
    *rpm = stubWheelRpm[WHEEL_RL];
    return stubWheelHealth[WHEEL_RL];
}

static CANRX_MESSAGE_health_E getWheelRrRpm(uint16_t* rpm)
{
    *rpm = stubWheelRpm[WHEEL_RR];
    return stubWheelHealth[WHEEL_RR];
}

const app_wheelSpeed_config_S app_wheelSpeed_config = {
    .sensorType        = {
        [WHEEL_FL] = WS_SENSORTYPE_CAN_RPM,
        [WHEEL_FR] = WS_SENSORTYPE_CAN_RPM,
        [WHEEL_RL] = WS_SENSORTYPE_CAN_RPM,
        [WHEEL_RR] = WS_SENSORTYPE_CAN_RPM,
    },
    .config            = {
        [WHEEL_FL].rpm = getWheelFlRpm,
        [WHEEL_FR].rpm = getWheelFrRpm,
        [WHEEL_RL].rpm = getWheelRlRpm,
        [WHEEL_RR].rpm = getWheelRrRpm,
    },
};

static float32_t rpmToMps(uint16_t rpm)
{
    return (((float32_t)rpm) / 60.0f) * WHEEL_CIRCUMFERENCE_M;
}

void setUp(void)
{
    for (uint8_t i = 0U; i < WHEEL_CNT; i++)
    {
        stubWheelRpm[i]    = 0U;
        stubWheelHealth[i] = CANRX_MESSAGE_VALID;
    }

    for (uint8_t i = 0U; i < HW_TIM_CHANNEL_WS_CNT; i++)
    {
        stubTimerHz[i] = 0.0f;
    }

    stubVehicleSpeed       = 0.0f;
    stubVehicleSpeedHealth = CANRX_MESSAGE_VALID;
    app_vehicleSpeed_desc.moduleInit();
}

void tearDown(void)
{
}

CANRX_MESSAGE_health_E CANRX_get_signal(CAN_bus_E bus, CAN_signal_E signal, void* value)
{
    (void)bus;

    if (signal == VCFRONT_vehicleSpeed)
    {
        *((float32_t*)value) = stubVehicleSpeed;
        return stubVehicleSpeedHealth;
    }

    return CANRX_MESSAGE_SNA;
}

uint32_t HW_TIM_getTimeMS(void)
{
    return 0U;
}

float32_t HW_TIM_getFreq(HW_TIM_channelFreq_E channel)
{
    return stubTimerHz[channel];
}

void test_init_clears_all_speed_outputs(void)
{
    TEST_ASSERT_EQUAL_UINT16(0U, app_vehicleSpeed_getWheelSpeedRawRotational(WHEEL_FL));
    TEST_ASSERT_EQUAL_UINT16(0U, app_vehicleSpeed_getWheelSpeedRotational(WHEEL_FL));
    TEST_ASSERT_EQUAL_UINT16(0U, app_vehicleSpeed_getAxleSpeedRotational(AXLE_FRONT));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, app_vehicleSpeed_getVehicleSpeed());
}

void test_follower_vehicle_speed_uses_valid_can_signal(void)
{
    stubVehicleSpeed = 12.5f;

    app_vehicleSpeed_desc.periodic100Hz_CLK();

    TEST_ASSERT_EQUAL_FLOAT(12.5f, app_vehicleSpeed_getVehicleSpeed());
}

void test_follower_vehicle_speed_falls_back_to_zero_when_can_signal_invalid(void)
{
    stubVehicleSpeed       = 12.5f;
    stubVehicleSpeedHealth = CANRX_MESSAGE_SNA;

    app_vehicleSpeed_desc.periodic100Hz_CLK();

    TEST_ASSERT_EQUAL_FLOAT(0.0f, app_vehicleSpeed_getVehicleSpeed());
}

void test_can_wheel_rpm_is_averaged_into_axle_speed(void)
{
    stubWheelRpm[WHEEL_FL] = 100U;
    stubWheelRpm[WHEEL_FR] = 120U;
    stubWheelRpm[WHEEL_RL] = 200U;
    stubWheelRpm[WHEEL_RR] = 240U;

    app_vehicleSpeed_desc.periodic100Hz_CLK();

    TEST_ASSERT_EQUAL_UINT16(110U, app_vehicleSpeed_getAxleSpeedRotational(AXLE_FRONT));
    TEST_ASSERT_EQUAL_UINT16(220U, app_vehicleSpeed_getAxleSpeedRotational(AXLE_REAR));
}

void test_raw_and_calculated_wheel_speed_match_when_sensor_is_valid(void)
{
    stubWheelRpm[WHEEL_RL] = 300U;

    app_vehicleSpeed_desc.periodic100Hz_CLK();

    TEST_ASSERT_EQUAL_UINT16(300U, app_vehicleSpeed_getWheelSpeedRawRotational(WHEEL_RL));
    TEST_ASSERT_EQUAL_UINT16(300U, app_vehicleSpeed_getWheelSpeedRotational(WHEEL_RL));
    TEST_ASSERT_EQUAL_FLOAT(rpmToMps(300U), app_vehicleSpeed_getWheelSpeedLinear(WHEEL_RL));
}

void test_degraded_wheel_falls_back_to_valid_same_axle_speed(void)
{
    stubWheelRpm[WHEEL_RL]    = 0U;
    stubWheelRpm[WHEEL_RR]    = 240U;
    stubWheelHealth[WHEEL_RL] = CANRX_MESSAGE_SNA;

    app_vehicleSpeed_desc.periodic100Hz_CLK();

    TEST_ASSERT_EQUAL_UINT16(0U,   app_vehicleSpeed_getWheelSpeedRawRotational(WHEEL_RL));
    TEST_ASSERT_EQUAL_UINT16(240U, app_vehicleSpeed_getWheelSpeedRotational(WHEEL_RL));
    TEST_ASSERT_EQUAL_UINT16(240U, app_vehicleSpeed_getAxleSpeedRotational(AXLE_REAR));
}

void test_degraded_axle_falls_back_to_other_valid_axle_speed(void)
{
    stubWheelRpm[WHEEL_FL]    = 100U;
    stubWheelRpm[WHEEL_FR]    = 140U;
    stubWheelHealth[WHEEL_RL] = CANRX_MESSAGE_SNA;
    stubWheelHealth[WHEEL_RR] = CANRX_MESSAGE_SNA;

    app_vehicleSpeed_desc.periodic100Hz_CLK();

    TEST_ASSERT_EQUAL_UINT16(120U, app_vehicleSpeed_getAxleSpeedRotational(AXLE_FRONT));
    TEST_ASSERT_EQUAL_UINT16(0U,   app_vehicleSpeed_getAxleSpeedRotational(AXLE_REAR));
    TEST_ASSERT_EQUAL_UINT16(120U, app_vehicleSpeed_getWheelSpeedRotational(WHEEL_RL));
    TEST_ASSERT_EQUAL_UINT16(120U, app_vehicleSpeed_getWheelSpeedRotational(WHEEL_RR));
}

void test_tire_slip_is_finite_when_vehicle_speed_is_nonzero(void)
{
    stubVehicleSpeed       = 10.0f;
    stubWheelRpm[WHEEL_RL] = 600U;

    app_vehicleSpeed_desc.periodic100Hz_CLK();

    TEST_ASSERT_TRUE(isfinite(app_vehicleSpeed_getTireSlip(WHEEL_RL)));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.25679f, app_vehicleSpeed_getTireSlip(WHEEL_RL));
}

void test_axle_slip_is_finite_when_vehicle_speed_is_nonzero(void)
{
    stubVehicleSpeed       = 10.0f;
    stubWheelRpm[WHEEL_RL] = 600U;
    stubWheelRpm[WHEEL_RR] = 600U;

    app_vehicleSpeed_desc.periodic100Hz_CLK();

    TEST_ASSERT_TRUE(isfinite(app_vehicleSpeed_getAxleSlip(AXLE_REAR)));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.25679f, app_vehicleSpeed_getAxleSlip(AXLE_REAR));
}

void test_tire_slip_is_finite_zero_when_tire_and_vehicle_are_stopped(void)
{
    stubVehicleSpeed       = 0.0f;
    stubWheelRpm[WHEEL_RL] = 0U;

    app_vehicleSpeed_desc.periodic100Hz_CLK();

    TEST_ASSERT_TRUE(isfinite(app_vehicleSpeed_getTireSlip(WHEEL_RL)));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, app_vehicleSpeed_getTireSlip(WHEEL_RL));
}

void test_tire_slip_is_finite_zero_when_tire_moves_and_vehicle_is_stopped(void)
{
    stubVehicleSpeed       = 0.0f;
    stubWheelRpm[WHEEL_RL] = 120U;

    app_vehicleSpeed_desc.periodic100Hz_CLK();

    TEST_ASSERT_TRUE(isfinite(app_vehicleSpeed_getTireSlip(WHEEL_RL)));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, app_vehicleSpeed_getTireSlip(WHEEL_RL));
}

void test_axle_slip_is_finite_zero_when_axle_and_vehicle_are_stopped(void)
{
    stubVehicleSpeed       = 0.0f;
    stubWheelRpm[WHEEL_RL] = 0U;
    stubWheelRpm[WHEEL_RR] = 0U;

    app_vehicleSpeed_desc.periodic100Hz_CLK();

    TEST_ASSERT_TRUE(isfinite(app_vehicleSpeed_getAxleSlip(AXLE_REAR)));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, app_vehicleSpeed_getAxleSlip(AXLE_REAR));
}

void test_axle_slip_is_finite_zero_when_axle_moves_and_vehicle_is_stopped(void)
{
    stubVehicleSpeed       = 0.0f;
    stubWheelRpm[WHEEL_RL] = 120U;
    stubWheelRpm[WHEEL_RR] = 120U;

    app_vehicleSpeed_desc.periodic100Hz_CLK();

    TEST_ASSERT_TRUE(isfinite(app_vehicleSpeed_getAxleSlip(AXLE_REAR)));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, app_vehicleSpeed_getAxleSlip(AXLE_REAR));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_clears_all_speed_outputs);
    RUN_TEST(test_follower_vehicle_speed_uses_valid_can_signal);
    RUN_TEST(test_follower_vehicle_speed_falls_back_to_zero_when_can_signal_invalid);
    RUN_TEST(test_can_wheel_rpm_is_averaged_into_axle_speed);
    RUN_TEST(test_raw_and_calculated_wheel_speed_match_when_sensor_is_valid);
    RUN_TEST(test_degraded_wheel_falls_back_to_valid_same_axle_speed);
    RUN_TEST(test_degraded_axle_falls_back_to_other_valid_axle_speed);
    RUN_TEST(test_tire_slip_is_finite_when_vehicle_speed_is_nonzero);
    RUN_TEST(test_axle_slip_is_finite_when_vehicle_speed_is_nonzero);
    RUN_TEST(test_tire_slip_is_finite_zero_when_tire_and_vehicle_are_stopped);
    RUN_TEST(test_tire_slip_is_finite_zero_when_tire_moves_and_vehicle_is_stopped);
    RUN_TEST(test_axle_slip_is_finite_zero_when_axle_and_vehicle_are_stopped);
    RUN_TEST(test_axle_slip_is_finite_zero_when_axle_moves_and_vehicle_is_stopped);
    return UNITY_END();
}
