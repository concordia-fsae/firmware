#include "lib_swFuse.h"
#include "HW_tim.h"
#include "unity.h"

uint32_t shared_code_test_hw_time_ms;

void setUp(void)
{
    shared_code_test_hw_setTimeMS(0U);
}

void tearDown(void)
{
}

static lib_swFuse_fuse_S make_fuse(void)
{
    const lib_swFuse_fuse_S fuse = {
        .config = {
            .overcurrent_threshold = 3.0f,
            .max_i2t = 10.0f,
            .over_energy_cooldown_ms = 100U,
        },
        .state = LIB_SWFUSE_INIT,
        .current_i2t = 0.0f,
        .last_run_ms = 0U,
    };

    return fuse;
}

static void init_resets_state_i2t_and_timer(void)
{
    lib_swFuse_fuse_S fuse = make_fuse();

    shared_code_test_hw_setTimeMS(123U);
    lib_swFuse_init(&fuse);

    TEST_ASSERT_EQUAL(LIB_SWFUSE_OK, lib_swFuse_getState(&fuse));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lib_swFuse_geti2t(&fuse));
    TEST_ASSERT_EQUAL_UINT32(123U, fuse.last_run_ms);
    TEST_ASSERT_EQUAL(DRV_TIMER_STOPPED, fuse.cooldown_timer.state);
}

static void overcurrent_accumulates_i2t_and_recovers_after_cooldown(void)
{
    lib_swFuse_fuse_S fuse = make_fuse();

    lib_swFuse_init(&fuse);
    shared_code_test_hw_setTimeMS(1000U);
    TEST_ASSERT_EQUAL(LIB_SWFUSE_OK, lib_swFuse_runCurrent(&fuse, 2.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lib_swFuse_geti2t(&fuse));

    shared_code_test_hw_setTimeMS(2000U);
    TEST_ASSERT_EQUAL(LIB_SWFUSE_OVERCURRENT, lib_swFuse_runCurrent(&fuse, 4.0f));
    TEST_ASSERT_TRUE(lib_swFuse_geti2t(&fuse) > 10.0f);

    shared_code_test_hw_setTimeMS(2099U);
    TEST_ASSERT_EQUAL(LIB_SWFUSE_OVERCURRENT, lib_swFuse_runCurrent(&fuse, 0.0f));
    shared_code_test_hw_setTimeMS(2100U);
    TEST_ASSERT_EQUAL(LIB_SWFUSE_OK, lib_swFuse_runCurrent(&fuse, 0.0f));
    TEST_ASSERT_EQUAL(DRV_TIMER_STOPPED, fuse.cooldown_timer.state);
}

static void unknown_state_is_left_unchanged(void)
{
    lib_swFuse_fuse_S fuse = make_fuse();

    fuse.state = (lib_swFuse_state_E)0xFFU;

    TEST_ASSERT_EQUAL((lib_swFuse_state_E)0xFFU, lib_swFuse_runCurrent(&fuse, 0.0f));
    TEST_ASSERT_EQUAL((lib_swFuse_state_E)0xFFU, lib_swFuse_getState(&fuse));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(init_resets_state_i2t_and_timer);
    RUN_TEST(overcurrent_accumulates_i2t_and_recovers_after_cooldown);
    RUN_TEST(unknown_state_is_left_unchanged);
    return UNITY_END();
}
