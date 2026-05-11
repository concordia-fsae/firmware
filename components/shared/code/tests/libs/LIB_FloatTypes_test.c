#include "LIB_FloatTypes.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void float_aliases_match_platform_float_widths(void)
{
    TEST_ASSERT_EQUAL_UINT(sizeof(float), sizeof(float32_t));
    TEST_ASSERT_EQUAL_UINT(sizeof(double), sizeof(float64_t));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(float_aliases_match_platform_float_widths);
    return UNITY_END();
}

