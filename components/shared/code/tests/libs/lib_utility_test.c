#include <math.h>
#include <stdint.h>
#include "lib_utility.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void natural_log_approximation_is_close_for_nominal_values(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.0f, ln(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, logf(2.0f), ln(2.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, logf(0.5f), ln(0.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, logf(10.0f), ln(10.0f));
}

static void utility_macros_count_and_saturate_values(void)
{
    uint8_t bytes[] = { 1U, 2U, 3U };
    uint32_t reg = 0xA5A50000UL;

    TEST_ASSERT_EQUAL_UINT(3U, COUNTOF(bytes));
    TEST_ASSERT_EQUAL_INT(5, SATURATE(0, 5, 10));
    TEST_ASSERT_EQUAL_INT(0, SATURATE(0, -5, 10));
    TEST_ASSERT_EQUAL_INT(10, SATURATE(0, 50, 10));

    SET_BIT(reg, 0x1UL);
    TEST_ASSERT_EQUAL_HEX32(0xA5A50001UL, reg);
    CLEAR_BIT(reg, 0x1UL);
    TEST_ASSERT_EQUAL_HEX32(0xA5A50000UL, reg);
    WRITE_REG(reg, 0x12345678UL);
    TEST_ASSERT_EQUAL_HEX32(0x12345678UL, READ_REG(reg));
    MODIFY_REG(reg, 0x00FF0000UL, 0x00550000UL);
    TEST_ASSERT_EQUAL_HEX32(0x12555678UL, reg);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(natural_log_approximation_is_close_for_nominal_values);
    RUN_TEST(utility_macros_count_and_saturate_values);
    return UNITY_END();
}

