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
} brakeTemp_RL;

static struct
{
    float32_t voltage;
    float32_t temp;
} brakeTemp_RR;


static lib_interpolation_point_S brakeTemp_RLMap[] = {
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

static lib_interpolation_point_S brakeTemp_RRMap[] = {
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





float32_t brakeTemp_getRLTemp(void)
{
    return brakeTemp_RL.temp;
}

float32_t brakeTemp_getRLVoltage(void)
{
    return brakeTemp_RL.voltage;
}

float32_t brakeTemp_getRRTemp(void)
{
    return brakeTemp_RR.temp;
}
float32_t brakeTemp_getRRVoltage(void)
{
    return brakeTemp_RR.voltage;
}

//default left saturation as false because idk what it does
static lib_interpolation_mapping_S brakeTemp_map1 = {
    .points = (lib_interpolation_point_S*)&brakeTemp_RLMap,
    .number_points = COUNTOF(brakeTemp_RLMap),
    .saturate_left = true,
    .saturate_right = true,
    .saturate_left = false,
    .saturate_right = false,
};

static lib_interpolation_mapping_S brakeTemp_map2 = {
    .points = (lib_interpolation_point_S*)&brakeTemp_RRMap,
    .number_points = COUNTOF(brakeTemp_RRMap),
    .saturate_left = true,
    .saturate_right = true,
    .saturate_left = false,
    .saturate_right = false,
};


static void brakeTemp_init(void)
{
    memset(&brakeTemp_RL, 0x00U, sizeof(brakeTemp_RL));
    lib_interpolation_init(&brakeTemp_map1, 0.0f);
    memset(&brakeTemp_RR, 0x00U, sizeof(brakeTemp_RR));
    lib_interpolation_init(&brakeTemp_map2, 0.0f);
}

static void brakeTemp_periodic_10Hz(void)
{
    brakeTemp_RL.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_L_BR_TEMP);
    brakeTemp_RR.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_R_BR_TEMP);
    brakeTemp_RL.temp = (lib_interpolation_interpolate(&brakeTemp_map1, brakeTemp_RL.voltage));
    brakeTemp_RR.temp = (lib_interpolation_interpolate(&brakeTemp_map2, brakeTemp_RR.voltage));

}

const ModuleDesc_S brakeTemp_desc = {
    .moduleInit = &brakeTemp_init,
    .periodic10Hz_CLK = &brakeTemp_periodic_10Hz,
};