#include <math.h>
#include "lib_madgwick.h"
#include "unity.h"

#define TEST_PI 3.14159265358979323846f

void setUp(void)
{
}

void tearDown(void)
{
}

static float quat_norm(const lib_madgwick_S* f)
{
    return sqrtf((f->q0 * f->q0) + (f->q1 * f->q1) + (f->q2 * f->q2) + (f->q3 * f->q3));
}

static void init_and_set_quaternion_normalize_state(void)
{
    lib_madgwick_S filter = { 0 };

    madgwick_init(&filter, 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, filter.q0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, filter.q1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, filter.q2);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, filter.q3);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, filter.beta);

    madgwick_set_quaternion(&filter, 2.0f, 0.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, filter.q0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, quat_norm(&filter));

    madgwick_set_quaternion(&filter, 0.0f, 0.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, filter.q0);
}

static void converts_euler_angles_between_radians_and_degrees(void)
{
    lib_madgwick_euler_S e = {
        .x = 180.0f,
        .y = 90.0f,
        .z = -45.0f,
    };

    madgwick_euler_deg_to_rad(&e);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, TEST_PI, e.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, TEST_PI / 2.0f, e.y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -TEST_PI / 4.0f, e.z);

    madgwick_euler_rad_to_deg(&e);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 180.0f, e.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 90.0f, e.y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -45.0f, e.z);
}

static void reports_euler_yaw_from_quaternion(void)
{
    lib_madgwick_S filter = { 0 };
    lib_madgwick_euler_S e = { 0 };

    madgwick_init(&filter, 0.1f);
    madgwick_set_quaternion(&filter, cosf(TEST_PI / 4.0f), 0.0f, 0.0f, sinf(TEST_PI / 4.0f));
    madgwick_get_euler_deg(&filter, &e);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, e.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, e.y);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, e.z);
}

static void reports_saturated_pitch_from_quaternion(void)
{
    lib_madgwick_S filter = { 0 };
    lib_madgwick_euler_S e = { 0 };

    madgwick_init(&filter, 0.1f);
    madgwick_set_quaternion(&filter, cosf(TEST_PI / 4.0f), 0.0f, sinf(TEST_PI / 4.0f), 0.0f);
    madgwick_get_euler_rad(&filter, &e);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, TEST_PI / 2.0f, e.y);
}

static void initializes_from_accel_edges(void)
{
    lib_madgwick_S filter = { 0 };
    const lib_madgwick_euler_S invalid_accel = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
    const lib_madgwick_euler_S inverted_accel = { .x = 0.0f, .y = 0.0f, .z = -1.0f };

    madgwick_init(&filter, 0.1f);
    madgwick_init_quaternion_from_accel(&filter, &invalid_accel);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, filter.q0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, filter.q1);

    madgwick_init_quaternion_from_accel(&filter, &inverted_accel);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, filter.q0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, filter.q1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, quat_norm(&filter));
}

static void imu_updates_cover_correction_and_gyro_only_paths(void)
{
    lib_madgwick_S filter = { 0 };
    const lib_madgwick_euler_S accel_up = { .x = 0.0f, .y = 0.0f, .z = 1.0f };
    const lib_madgwick_euler_S accel_side = { .x = 1.0f, .y = 0.0f, .z = 0.0f };
    const lib_madgwick_euler_S invalid_accel = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
    const lib_madgwick_euler_S gyro_z = { .x = 0.0f, .y = 0.0f, .z = 90.0f };
    lib_madgwick_euler_S e = { 0 };

    madgwick_init(&filter, 0.1f);
    madgwick_init_quaternion_from_accel(&filter, &accel_up);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, quat_norm(&filter));

    madgwick_update_imu(&filter, &gyro_z, &accel_side, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, quat_norm(&filter));

    madgwick_update_imu(&filter, &gyro_z, &invalid_accel, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, quat_norm(&filter));
    madgwick_get_euler_deg(&filter, &e);
    TEST_ASSERT_TRUE(e.z > 1.0f);
}

static void mag_updates_cover_valid_and_fallback_paths(void)
{
    lib_madgwick_S filter = { 0 };
    const lib_madgwick_euler_S accel_up = { .x = 0.0f, .y = 0.0f, .z = 1.0f };
    const lib_madgwick_euler_S accel_side = { .x = 1.0f, .y = 0.0f, .z = 0.0f };
    const lib_madgwick_euler_S invalid_accel = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
    const lib_madgwick_euler_S gyro_z = { .x = 0.0f, .y = 0.0f, .z = 90.0f };
    const lib_madgwick_euler_S mag_x = { .x = 1.0f, .y = 0.0f, .z = 0.0f };
    const lib_madgwick_euler_S mag_y = { .x = 0.0f, .y = 1.0f, .z = 0.0f };
    const lib_madgwick_euler_S invalid_mag = { .x = 0.0f, .y = 0.0f, .z = 0.0f };

    madgwick_init(&filter, 0.1f);
    madgwick_update_mag(&filter, &gyro_z, &accel_up, &invalid_mag, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, quat_norm(&filter));

    madgwick_update_mag(&filter, &gyro_z, &invalid_accel, &mag_x, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, quat_norm(&filter));

    madgwick_update_mag(&filter, &gyro_z, &accel_up, &mag_x, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, quat_norm(&filter));

    madgwick_update_mag(&filter, &gyro_z, &accel_side, &mag_y, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, quat_norm(&filter));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(init_and_set_quaternion_normalize_state);
    RUN_TEST(converts_euler_angles_between_radians_and_degrees);
    RUN_TEST(reports_euler_yaw_from_quaternion);
    RUN_TEST(reports_saturated_pitch_from_quaternion);
    RUN_TEST(initializes_from_accel_edges);
    RUN_TEST(imu_updates_cover_correction_and_gyro_only_paths);
    RUN_TEST(mag_updates_cover_valid_and_fallback_paths);
    return UNITY_END();
}
