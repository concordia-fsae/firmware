/**
 * @file shockpot.c
 * @brief Module source for steering angle sensor
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "shockpot.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "drv_inputAD_componentSpecific.h"
#include "drv_inputAD.h"
#include "MessageUnpack_generated.h"
#include "lib_interpolation.h"
#include "HW_adc_componentSpecific.h"

static struct
{
    float32_t voltage;
    float32_t disp;
} shockpot_L;

static struct
{
    float32_t voltage;
    float32_t disp;
} shockpot_R;

static lib_interpolation_point_S shockpot_LMap[] = {
    {
        .x = 0.3f, // voltage
        .y = 75.0f, 
    },
    {
        .x = 2.7f, // sensor reference voltage
        .y = 0.0f, 
    },
};

static lib_interpolation_point_S shockpot_RMap[] = {
    {
        .x = 0.3f, // voltage
        .y = 75.0f, 
    },
    {
        .x = 2.7f, // sensor reference voltage
        .y = 0.0f, 
    },
};

float32_t shockpot_getLDisp(void)
{
    return shockpot_L.disp;
}

float32_t shockpot_getLVoltage(void)
{
    return shockpot_L.voltage;
}

float32_t shockpot_getRDisp(void)
{
    return shockpot_R.disp;
}
float32_t shockpot_getRVoltage(void)
{
    return shockpot_R.voltage;
}


static lib_interpolation_mapping_S shockpot_map1 = {
    .points = (lib_interpolation_point_S*)&shockpot_LMap,
    .number_points = COUNTOF(shockpot_LMap),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_mapping_S shockpot_map2 = {
    .points = (lib_interpolation_point_S*)&shockpot_RMap,
    .number_points = COUNTOF(shockpot_RMap),
    .saturate_left = true,
    .saturate_right = true,
};

static void shockpot_init(void)
{
    memset(&shockpot_L, 0x00U, sizeof(shockpot_L));
    lib_interpolation_init(&shockpot_map1, 0.0f);
    memset(&shockpot_R, 0x00U, sizeof(shockpot_R));
    lib_interpolation_init(&shockpot_map2, 0.0f);
}

static void shockpot_periodic_100Hz(void)
{
    shockpot_L.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_L_SHK_DISP);
    shockpot_R.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_R_SHK_DISP);
    shockpot_L.disp = (lib_interpolation_interpolate(&shockpot_map1, shockpot_L.voltage));
    shockpot_R.disp = (lib_interpolation_interpolate(&shockpot_map2, shockpot_R.voltage));

}

const ModuleDesc_S shockpot_desc = {
    .moduleInit = &shockpot_init,
    .periodic100Hz_CLK = &shockpot_periodic_100Hz,
};
