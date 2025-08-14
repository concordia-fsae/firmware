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
} shockpot_FL;

static struct
{
    float32_t voltage;
    float32_t disp;
} shockpot_FR;

static lib_interpolation_point_S shockpot_FLMap[] = {
    {
        .x = 0.3f, // voltage
        .y = 75.0f, 
    },
    {
        .x = 2.7f, // sensor reference voltage
        .y = 0.0f, 
    },
};

static lib_interpolation_point_S shockpot_FRMap[] = {
    {
        .x = 0.3f, // voltage
        .y = 75.0f, 
    },
    {
        .x = 2.7f, // sensor reference voltage
        .y = 0.0f, 
    },
};

float32_t shockpot_getFLDisp(void) 
{
    return shockpot_FL.disp;
}

float32_t shockpot_getFLVoltage(void) 
{
    return shockpot_FL.voltage;
}

float32_t shockpot_getFRDisp(void) 
{
    return shockpot_FR.disp;
}
float32_t shockpot_getFRVoltage(void) 
{
    return shockpot_FR.voltage;
}


static lib_interpolation_mapping_S shockpot_map1 = {
    .points = (lib_interpolation_point_S*)&shockpot_FLMap,
    .number_points = COUNTOF(shockpot_FLMap),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_mapping_S shockpot_map2 = {
    .points = (lib_interpolation_point_S*)&shockpot_FRMap,
    .number_points = COUNTOF(shockpot_FRMap),
    .saturate_left = true,
    .saturate_right = true,
};
static void shockpot_init(void)
{
    memset(&shockpot_FL, 0x00U, sizeof(shockpot_FL)); 
    lib_interpolation_init(&shockpot_map1, 0.0f);
    memset(&shockpot_FR, 0x00U, sizeof(shockpot_FR)); 
    lib_interpolation_init(&shockpot_map2, 0.0f);
}

static void shockpot_periodic_10Hz(void)
{
    shockpot_FL.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_L_SHK_DISP);
    shockpot_FR.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_R_SHK_DISP);
    shockpot_FL.disp = (lib_interpolation_interpolate(&shockpot_map1, shockpot_FL.voltage));
    shockpot_FR.disp = (lib_interpolation_interpolate(&shockpot_map2, shockpot_FR.voltage));

}

const ModuleDesc_S shockpot_desc = {
    .moduleInit = &shockpot_init,
    .periodic10Hz_CLK = &shockpot_periodic_10Hz,
};
