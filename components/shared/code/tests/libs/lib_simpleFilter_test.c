#include "lib_simpleFilter.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void cumulative_average_tracks_integer_samples(void)
{
    lib_simpleFilter_cumAvg_S filter = {
        .raw = 10U,
        .value = 1.0f,
        .count = 2U,
    };

    lib_simpleFilter_cumAvg_clear(&filter);
    TEST_ASSERT_EQUAL_UINT32(0U, filter.raw);
    TEST_ASSERT_EQUAL_UINT16(0U, filter.count);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lib_simpleFilter_cumAvg_average(&filter));

    lib_simpleFilter_cumAvg_increment(&filter, 10U);
    lib_simpleFilter_cumAvg_increment(&filter, 20U);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, lib_simpleFilter_cumAvg_average(&filter));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, filter.value);
}

static void cumulative_average_tracks_float_samples(void)
{
    lib_simpleFilter_cumAvgF_S filter = {
        .raw = 10.0f,
        .value = 1.0f,
        .count = 2U,
    };

    lib_simpleFilter_cumAvgF_clear(&filter);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, filter.raw);
    TEST_ASSERT_EQUAL_UINT16(0U, filter.count);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lib_simpleFilter_cumAvgF_average(&filter));

    lib_simpleFilter_cumAvgF_increment(&filter, 1.5f);
    lib_simpleFilter_cumAvgF_increment(&filter, 2.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, lib_simpleFilter_cumAvgF_average(&filter));
}

static void lpf_uses_weighted_average(void)
{
    lib_simpleFilter_lpf_S filter = {
        .smoothing_factor = 0.0f,
        .y = 10.0f,
        .y_1 = 0.0f,
    };
    float out = 0.0f;
    const float a = 8.0f;
    const float b = 2.0f;

    LIB_SIMPLEFILTER_WEIGHTAVG(&a, &b, 0.25f, &out);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.5f, out);

    lib_simpleFilter_lpf_calcSmoothingFactor(&filter, 2.0f, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, filter.smoothing_factor);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, lib_simpleFilter_lpf_step(&filter, 20.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, filter.y_1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(cumulative_average_tracks_integer_samples);
    RUN_TEST(cumulative_average_tracks_float_samples);
    RUN_TEST(lpf_uses_weighted_average);
    return UNITY_END();
}

