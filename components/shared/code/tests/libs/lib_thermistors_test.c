#include "lib_thermistors.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void returns_nominal_temperature_at_reference_resistance(void)
{
    const float32_t kelvin = lib_thermistors_getKelvinFromR_BParameter(&NCP21_bParam, NCP21_bParam.R0);
    const float32_t celsius = lib_thermistors_getCelsiusFromR_BParameter(&NCP21_bParam, NCP21_bParam.R0);

    TEST_ASSERT_FLOAT_WITHIN(0.2f, NCP21_bParam.T0, kelvin);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 25.0f, celsius);
}

static void higher_ntc_resistance_maps_to_lower_temperature(void)
{
    const float32_t nominal = lib_thermistors_getKelvinFromR_BParameter(&MF52_bParam, MF52_bParam.R0);
    const float32_t cooler = lib_thermistors_getKelvinFromR_BParameter(&MF52_bParam, MF52_bParam.R0 * 2.0f);

    TEST_ASSERT_TRUE(cooler < nominal);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(returns_nominal_temperature_at_reference_resistance);
    RUN_TEST(higher_ntc_resistance_maps_to_lower_temperature);
    return UNITY_END();
}

