#include <stdint.h>
#include "Utility.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void counts_leading_zeroes_and_reverses_bytes(void)
{
    uint8_t data[] = { 0x12U, 0x34U, 0x56U, 0x78U, 0x9AU };

    TEST_ASSERT_EQUAL_UINT16(32U, u32CountLeadingZeroes(0U));
    TEST_ASSERT_EQUAL_UINT16(0U, u32CountLeadingZeroes(0x80000000UL));
    TEST_ASSERT_EQUAL_UINT16(7U, u32CountLeadingZeroes(0x01000000UL));
    TEST_ASSERT_EQUAL_UINT8(0x80U, reverse_byte(0x01U));
    TEST_ASSERT_EQUAL_UINT8(0x69U, reverse_byte(0x96U));

    TEST_ASSERT_EQUAL_PTR(data, reverse_bytes(data, (uint8_t)COUNTOF(data)));
    TEST_ASSERT_EQUAL_UINT8(0x9AU, data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x78U, data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x56U, data[2]);
    TEST_ASSERT_EQUAL_UINT8(0x34U, data[3]);
    TEST_ASSERT_EQUAL_UINT8(0x12U, data[4]);
}

static void natural_log_approximation_is_close_for_nominal_values(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.0f, ln(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.693147f, ln(2.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.693147f, ln(0.5f));
}

static void flag_helpers_cover_single_and_multiword_sets(void)
{
    FLAG_create(flags, 20U) = { 0U };

    TEST_ASSERT_TRUE(FLAG_none(flags, 20U));
    TEST_ASSERT_FALSE(FLAG_any(flags, 20U));

    FLAG_set(flags, 3U);
    FLAG_set(flags, 17U);
    TEST_ASSERT_TRUE(FLAG_get(flags, 3U));
    TEST_ASSERT_TRUE(FLAG_get(flags, 17U));
    TEST_ASSERT_EQUAL_UINT16(3U, FLAG_getFirst(flags, 20U));
    TEST_ASSERT_EQUAL_UINT16(17U, FLAG_getNext(flags, 20U, 4U));
    TEST_ASSERT_EQUAL_UINT16(20U, FLAG_getNext(flags, 20U, 18U));
    TEST_ASSERT_TRUE(FLAG_any(flags, 20U));
    TEST_ASSERT_FALSE(FLAG_all(flags, 20U));

    FLAG_assign(flags, 3U, false);
    FLAG_or(flags, 4U, true);
    TEST_ASSERT_FALSE(FLAG_get(flags, 3U));
    TEST_ASSERT_TRUE(FLAG_get(flags, 4U));

    FLAG_setAll(flags, 20U);
    TEST_ASSERT_TRUE(FLAG_all(flags, 20U));
    TEST_ASSERT_EQUAL_HEX16(0xFFFFU, flags[0]);
    TEST_ASSERT_EQUAL_HEX16(0x000FU, flags[1]);

    FLAG_clear(flags, 17U);
    TEST_ASSERT_FALSE(FLAG_all(flags, 20U));
    FLAG_clearAll(flags, 20U);
    TEST_ASSERT_TRUE(FLAG_none(flags, 20U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(counts_leading_zeroes_and_reverses_bytes);
    RUN_TEST(natural_log_approximation_is_close_for_nominal_values);
    RUN_TEST(flag_helpers_cover_single_and_multiword_sets);
    return UNITY_END();
}
