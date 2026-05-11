#include "lib_voltageDivider.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void computes_unknown_resistance_from_known_pullup(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f,
                             10000.0f,
                             lib_voltageDivider_getRFromVKnownPullUp(2.5f, 10000.0f, 5.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f,
                             5000.0f,
                             lib_voltageDivider_getRFromVKnownPullUp(1.6666667f, 10000.0f, 5.0f));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(computes_unknown_resistance_from_known_pullup);
    return UNITY_END();
}

