#include "lib_pid.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void init_resets_terms_and_keeps_previous_output(void)
{
    lib_pid_S pid = { 0 };

    lib_pid_init(&pid, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, pid.kp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, pid.ki);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, pid.kd);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, pid.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, pid.x_1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid.p_term);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid.i_term);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid.d_term);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, pid.y);
}

static void pi_and_pid_typeb_calculate_terms_and_clamp_output(void)
{
    lib_pid_S pid = { 0 };

    lib_pid_init(&pid, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f);
    lib_pid_typeb_calc(&pid, 1.0f, 3.0f, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, pid.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, pid.p_term);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, pid.i_term);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.0f, pid.d_term);

    lib_pid_typeb_sum(&pid, -5.0f, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, pid.y);
    pid.d_term = -20.0f;
    lib_pid_typeb_sum(&pid, -5.0f, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -5.0f, pid.y);
    pid.d_term = 0.0f;
    lib_pid_typeb_sum(&pid, -5.0f, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, pid.y);
}

static void integral_utilities_limit_leak_and_filter_derivative(void)
{
    lib_pid_S pid = { 0 };

    lib_pid_init(&pid, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    pid.i_term = 10.0f;
    lib_pid_util_ilim(&pid, -2.0f, 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, pid.i_term);
    pid.i_term = -10.0f;
    lib_pid_util_ilim(&pid, -2.0f, 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -2.0f, pid.i_term);

    pid.i_term = 10.0f;
    lib_pid_util_ileak(&pid, 0.25f, 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, pid.i_term);

    lib_pid_util_lpf_dTermSetCutoff(&pid, 2.0f);
    pid.d_term = 20.0f;
    pid.filterDTerm.y = 10.0f;
    lib_pid_util_lpf_dTerm(&pid, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, pid.d_term);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(init_resets_terms_and_keeps_previous_output);
    RUN_TEST(pi_and_pid_typeb_calculate_terms_and_clamp_output);
    RUN_TEST(integral_utilities_limit_leak_and_filter_derivative);
    return UNITY_END();
}

