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
        .x = 0.83f, // on the datasheet it reads 0.5 but the function outputs on a 3.3v scale and to change
                        //it to a 5V scale I need to multiply by (3/5)
        .y = 0.0f,
    },
    {
        .x = 2.7f, // voltage
        .y = 800.0f,
    },

};

static lib_interpolation_point_S brakeTemp_RRMap[] = {
    {
        .x = 0.83f, // sensor reference voltage
        .y = 0.0f,
    },
    {
        .x = 2.7f, // voltage
        .y = 800.0f,
    }

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


static lib_interpolation_mapping_S brakeTemp_map1 = {
    .points = (lib_interpolation_point_S*)&brakeTemp_RLMap,
    .number_points = COUNTOF(brakeTemp_RLMap),
    .saturate_left = true,
    .saturate_right = true,


};

static lib_interpolation_mapping_S brakeTemp_map2 = {
    .points = (lib_interpolation_point_S*)&brakeTemp_RRMap,
    .number_points = COUNTOF(brakeTemp_RRMap),
    .saturate_left = true,
    .saturate_right = true,


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