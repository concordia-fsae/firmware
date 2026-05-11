#include <stdbool.h>
#include <stdint.h>
#include "LIB_Types.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void common_integer_bool_and_float_types_are_available(void)
{
    bool b = true;
    uint8_t u8 = 0xAAU;
    uint16_t u16 = 0xA5A5U;
    uint32_t u32 = 0xA5A5A5A5UL;
    float32_t f = 1.25f;

    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_EQUAL_UINT8(0xAAU, u8);
    TEST_ASSERT_EQUAL_UINT16(0xA5A5U, u16);
    TEST_ASSERT_EQUAL_UINT32(0xA5A5A5A5UL, u32);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.25f, f);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(common_integer_bool_and_float_types_are_available);
    return UNITY_END();
}

