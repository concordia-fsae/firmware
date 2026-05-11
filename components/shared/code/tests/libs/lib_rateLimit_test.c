#include "lib_rateLimit.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void limits_positive_and_negative_steps(void)
{
    lib_rateLimit_linear_S linear = {
        .y_n = 0.0f,
        .maxStepDelta = 2.5f,
    };

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, lib_rateLimit_linear_update(&linear, 10.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, lib_rateLimit_linear_update(&linear, 10.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, lib_rateLimit_linear_update(&linear, -10.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lib_rateLimit_linear_update(&linear, -10.0f));
}

static void snaps_to_input_when_inside_step_delta(void)
{
    lib_rateLimit_linear_S linear = {
        .y_n = 5.0f,
        .maxStepDelta = 2.5f,
    };

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, lib_rateLimit_linear_update(&linear, 6.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, lib_rateLimit_linear_update(&linear, 4.0f));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(limits_positive_and_negative_steps);
    RUN_TEST(snaps_to_input_when_inside_step_delta);
    return UNITY_END();
}

