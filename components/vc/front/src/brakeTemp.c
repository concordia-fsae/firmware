/**
 * @file brakeTemp.c
 * @brief Module source for brakeTemp sensor
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "brakeTemp.h"
//copied code
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
    float32_t temp;
} brakeTemp_FL;

static struct
{
    float32_t voltage;
    float32_t temp;
} brakeTemp_FR;


static lib_interpolation_point_S brakeTemp_FLMap[] = {
    {
        .x = 0.5f, // sensor reference voltage
        .y = 0.0f,
    },
    {
        .x = 0.6f, // voltage
        .y = 20.0f,
    },
    {
        .x = 0.7f, // voltage
        .y = 40.0f,
    },
    {
        .x = 0.8f, // voltage
        .y = 60.0f,
    },
    {
        .x = 0.9f, // voltage
        .y = 80.0f,
    },
    {
        .x = 1.0f, // voltage
        .y = 100.0f,
    },
    {
        .x = 1.10f, // voltage
        .y = 120.0f,
    },
    {
        .x = 1.2f, // voltage
        .y = 140.0f,
    },
    {
        .x = 1.3f, // voltage
        .y = 160.0f,
    },
    {
        .x = 1.4f, // voltage
        .y = 180.0f,
    },
    {
        .x = 1.5f, // voltage
        .y = 200.0f,
    },
    {
        .x = 2.0f, // voltage
        .y = 300.0f,
    },
    {
        .x = 2.5f, // voltage
        .y = 400.0f,
    },
    {
        .x = 3.0f, // voltage
        .y = 500.0f,
    },
    {
        .x = 3.5f, // voltage
        .y = 600.0f,
    },
    {
        .x = 4.0f, // voltage
        .y = 700.0f,
    },
    {
        .x = 4.5f, // voltage
        .y = 800.0f,
    },

};

static lib_interpolation_point_S brakeTemp_FRMap[] = {
    {
        .x = 0.5f, // sensor reference voltage
        .y = 0.0f,
    },
    {
        .x = 0.6f, // voltage
        .y = 20.0f,
    },
    {
        .x = 0.7f, // voltage
        .y = 40.0f,
    },
    {
        .x = 0.8f, // voltage
        .y = 60.0f,
    },
    {
        .x = 0.9f, // voltage
        .y = 80.0f,
    },
    {
        .x = 1.0f, // voltage
        .y = 100.0f,
    },
    {
        .x = 1.10f, // voltage
        .y = 120.0f,
    },
    {
        .x = 1.2f, // voltage
        .y = 140.0f,
    },
    {
        .x = 1.3f, // voltage
        .y = 160.0f,
    },
    {
        .x = 1.4f, // voltage
        .y = 180.0f,
    },
    {
        .x = 1.5f, // voltage
        .y = 200.0f,
    },
    {
        .x = 2.0f, // voltage
        .y = 300.0f,
    },
    {
        .x = 2.5f, // voltage
        .y = 400.0f,
    },
    {
        .x = 3.0f, // voltage
        .y = 500.0f,
    },
    {
        .x = 3.5f, // voltage
        .y = 600.0f,
    },
    {
        .x = 4.0f, // voltage
        .y = 700.0f,
    },
    {
        .x = 4.5f, // voltage
        .y = 800.0f,
    },

};





float32_t brakeTemp_getFLTemp(void)
{
    return brakeTemp_FL.temp;
}

float32_t brakeTemp_getFLVoltage(void)
{
    return brakeTemp_FL.voltage;
}

float32_t brakeTemp_getFRTemp(void)
{
    return brakeTemp_FR.temp;
}
float32_t brakeTemp_getFRVoltage(void)
{
    return brakeTemp_FR.voltage;
}

//default left saturation as false because idk what it does
static lib_interpolation_mapping_S brakeTemp_map1 = {
    .points = (lib_interpolation_point_S*)&brakeTemp_FLMap,
    .number_points = COUNTOF(brakeTemp_FLMap),
    .saturate_left = true,
    .saturate_right = true,
    .saturate_left = false,
    .saturate_right = false,
};

static lib_interpolation_mapping_S brakeTemp_map2 = {
    .points = (lib_interpolation_point_S*)&brakeTemp_FRMap,
    .number_points = COUNTOF(brakeTemp_FRMap),
    .saturate_left = true,
    .saturate_right = true,
    .saturate_left = false,
    .saturate_right = false,

};


static void brakeTemp_init(void)
{
    memset(&brakeTemp_FL, 0x00U, sizeof(brakeTemp_FL));
    lib_interpolation_init(&brakeTemp_map1, 0.0f);
    memset(&brakeTemp_FR, 0x00U, sizeof(brakeTemp_FR));
    lib_interpolation_init(&brakeTemp_map2, 0.0f);
}

static void brakeTemp_periodic_10Hz(void)
{
    brakeTemp_FL.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_L_SHK_DISP);
    brakeTemp_FR.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_R_SHK_DISP);
    brakeTemp_FL.temp = (lib_interpolation_interpolate(&brakeTemp_map1, brakeTemp_FL.voltage));
    brakeTemp_FR.temp = (lib_interpolation_interpolate(&brakeTemp_map2, brakeTemp_FR.voltage));

}

const ModuleDesc_S brakeTemp_desc = {
    .moduleInit = &brakeTemp_init,
    .periodic10Hz_CLK = &brakeTemp_periodic_10Hz,
};