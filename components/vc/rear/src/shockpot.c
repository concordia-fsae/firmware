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
} shockpot_RL;

static struct
{
    float32_t voltage;
    float32_t disp;
} shockpot_RR;

static lib_interpolation_point_S shockpot_RLMap[] = {
    {
        .x = 2.70f, // sensor reference voltage
        .y = 0.0f, // left turned degrees
    },
    {
        .x = 0.15f, // voltage
        .y = 3977.0f, // right turned degrees
    },
};

static lib_interpolation_point_S shockpot_RRMap[] = {
    {
        .x = 2.7f, // voltage
        .y = 0.0f, 
    },
    {
        .x = 0.1f, // voltage
        .y = 3977.0f, 
    },
};

float32_t shockpot_getRLDisp(void) 
{
    return shockpot_RL.disp;
}

float32_t shockpot_getRLVoltage(void) 
{
    return shockpot_RL.voltage;
}

float32_t shockpot_getRRDisp(void) 
{
    return shockpot_RR.disp;
}
float32_t shockpot_getRRVoltage(void) 
{
    return shockpot_RR.voltage;
}


static lib_interpolation_mapping_S shockpot_map1 = {
    .points = (lib_interpolation_point_S*)&shockpot_RLMap,
    .number_points = COUNTOF(shockpot_RLMap),
    .saturate_left = false,
    .saturate_right = false,
};

static lib_interpolation_mapping_S shockpot_map2 = {
    .points = (lib_interpolation_point_S*)&shockpot_RRMap,
    .number_points = COUNTOF(shockpot_RRMap),
    .saturate_left = false,
    .saturate_right = false,
};
static void shockpot_init(void)
{
    memset(&shockpot_RL, 0x00U, sizeof(shockpot_RL)); 
    lib_interpolation_init(&shockpot_map1, 0.0f);
    memset(&shockpot_RR, 0x00U, sizeof(shockpot_RR)); 
    lib_interpolation_init(&shockpot_map2, 0.0f);
}

static void shockpot_periodic_10Hz(void)
{
    shockpot_RL.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_L_SHK_DISP);
    shockpot_RR.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_R_SHK_DISP);
    shockpot_RL.disp = (lib_interpolation_interpolate(&shockpot_map1, shockpot_RL.voltage));
    shockpot_RR.disp = (lib_interpolation_interpolate(&shockpot_map2, shockpot_RR.voltage));

}

const ModuleDesc_S shockpot_desc = {
    .moduleInit = &shockpot_init,
    .periodic10Hz_CLK = &shockpot_periodic_10Hz,
};