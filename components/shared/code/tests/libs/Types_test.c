#include <stdbool.h>
#include <stdint.h>
#include "Types.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void c99_integer_and_bool_types_are_reexported(void)
{
    bool b = false;
    uint8_t u8 = 7U;
    uint16_t u16 = 700U;
    uint32_t u32 = 70000UL;

    TEST_ASSERT_FALSE(b);
    TEST_ASSERT_EQUAL_UINT8(7U, u8);
    TEST_ASSERT_EQUAL_UINT16(700U, u16);
    TEST_ASSERT_EQUAL_UINT32(70000UL, u32);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(c99_integer_and_bool_types_are_reexported);
    return UNITY_END();
}

