//

// Rear brake temperature module

//
#include "Module.h"
#include "ModuleDesc.h"
#include "drv_inputAD_componentSpecific.h"
#include "drv_inputAD.h"
#include "MessageUnpack_generated.h"
#include "lib_interpolation.h"
#include "HW_adc_componentSpecific.h"
#include "brakeTemp.h"

static struct

{

    float32_t voltage;

    float32_t Temp;

} brakeTemp_RL, brakeTemp_RR;


static lib_interpolation_point_S brakeTemp_RLPoints[] = {

    { .x = 0.83f, .y = 0.0f   },  // example: ~0.83 V -> 0 °C

    { .x = 2.7f,  .y = 800.0f },  // 2.7 V -> 800 °C

};

static lib_interpolation_point_S brakeTemp_RRPoints[] = {

    { .x = 0.83f, .y = 0.0f   },

    { .x = 2.7f,  .y = 800.0f },

};

static lib_interpolation_mapping_S brakeTemp_RLmap = {

    .points         = brakeTemp_RLPoints,

    .number_points  = COUNTOF(brakeTemp_RLPoints),

    .saturate_left  = true,

    .saturate_right = true,

};

static lib_interpolation_mapping_S brakeTemp_RRmap = {
    .points         = brakeTemp_RRPoints,
    .number_points  = COUNTOF(brakeTemp_RRPoints),
    .saturate_left  = true,
    .saturate_right = true,
};
float32_t brakeTemp_getRLtemp(void)

{
    return brakeTemp_RL.Temp;
}

float32_t brakeTemp_getRLVoltage(void)

{
    return brakeTemp_RL.voltage;
}

float32_t brakeTemp_getRRtemp(void)

{
    return brakeTemp_RR.Temp;
}

float32_t brakeTemp_getRRVoltage(void)

{
    return brakeTemp_RR.voltage;
}


static void brakeTemp_init(void)

{

    memset(&brakeTemp_RL, 0, sizeof(brakeTemp_RL));
    memset(&brakeTemp_RR, 0, sizeof(brakeTemp_RR));
    lib_interpolation_init(&brakeTemp_RLmap, 0.0f);
    lib_interpolation_init(&brakeTemp_RRmap, 0.0f);
}

static void brakeTemp_periodic_10Hz(void)

{
    brakeTemp_RL.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_L_BR_TEMP);

    brakeTemp_RR.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_R_BR_TEMP);

    brakeTemp_RL.Temp = lib_interpolation_interpolate(&brakeTemp_RLmap, brakeTemp_RL.voltage);

    brakeTemp_RR.Temp = lib_interpolation_interpolate(&brakeTemp_RRmap, brakeTemp_RR.voltage);

}

const ModuleDesc_S brakeTemp_desc = {

    .moduleInit       = &brakeTemp_init,

    .periodic10Hz_CLK = &brakeTemp_periodic_10Hz,

};

