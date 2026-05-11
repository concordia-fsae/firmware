#include "lib_interpolation.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void initializes_result_and_interpolates_between_points(void)
{
    lib_interpolation_point_S points[] = {
        { .x = 0.0f, .y = 0.0f },
        { .x = 10.0f, .y = 100.0f },
        { .x = 20.0f, .y = 300.0f },
    };
    lib_interpolation_mapping_S mapping = {
        .points = points,
        .number_points = (uint8_t)(sizeof(points) / sizeof(points[0])),
        .saturate_left = true,
        .saturate_right = true,
        .result = 0.0f,
    };

    lib_interpolation_init(&mapping, 42.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f, mapping.result);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, lib_interpolation_interpolate(&mapping, 5.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f, lib_interpolation_interpolate(&mapping, 15.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lib_interpolation_interpolate(&mapping, -10.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 300.0f, lib_interpolation_interpolate(&mapping, 30.0f));
}

static void extrapolates_when_saturation_is_disabled(void)
{
    lib_interpolation_point_S points[] = {
        { .x = 0.0f, .y = 0.0f },
        { .x = 10.0f, .y = 100.0f },
    };
    lib_interpolation_mapping_S mapping = {
        .points = points,
        .number_points = (uint8_t)(sizeof(points) / sizeof(points[0])),
        .saturate_left = false,
        .saturate_right = false,
        .result = 0.0f,
    };

    TEST_ASSERT_FLOAT_WITHIN(0.001f, -50.0f, lib_interpolation_interpolate(&mapping, -5.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 150.0f, lib_interpolation_interpolate(&mapping, 15.0f));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(initializes_result_and_interpolates_between_points);
    RUN_TEST(extrapolates_when_saturation_is_disabled);
    return UNITY_END();
}

