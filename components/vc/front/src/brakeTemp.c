//
// Front brake temperature module
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
} brakeTemp_FL, brakeTemp_FR;


static lib_interpolation_point_S brakeTemp_FLPoints[] = {
    { .x = 0.83f, .y = 0.0f   },
    { .x = 2.7f,  .y = 800.0f },
};

static lib_interpolation_point_S brakeTemp_FRPoints[] = {
    { .x = 0.83f, .y = 0.0f   },
    { .x = 2.7f,  .y = 800.0f },
};

static lib_interpolation_mapping_S brakeTemp_FLmap = {
    .points         = brakeTemp_FLPoints,
    .number_points  = COUNTOF(brakeTemp_FLPoints),
    .saturate_left  = true,
    .saturate_right = true,
};

static lib_interpolation_mapping_S brakeTemp_FRmap = {
    .points         = brakeTemp_FRPoints,
    .number_points  = COUNTOF(brakeTemp_FRPoints),
    .saturate_left  = true,
    .saturate_right = true,
};


float32_t brakeTemp_getFLtemp(void)
{
    return brakeTemp_FL.Temp;
}

float32_t brakeTemp_getFLVoltage(void)
{
    return brakeTemp_FL.voltage;
}

float32_t brakeTemp_getFRtemp(void)
{
    return brakeTemp_FR.Temp;
}

float32_t brakeTemp_getFRVoltage(void)
{
    return brakeTemp_FR.voltage;
}

static void brakeTemp_init(void)
{
    memset(&brakeTemp_FL, 0, sizeof(brakeTemp_FL));
    memset(&brakeTemp_FR, 0, sizeof(brakeTemp_FR));

    lib_interpolation_init(&brakeTemp_FLmap, 0.0f);
    lib_interpolation_init(&brakeTemp_FRmap, 0.0f);
}

static void brakeTemp_periodic_10Hz(void)
{
    /* Use correct ADC enums */
    brakeTemp_FL.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_L_SHK_DISP);
    brakeTemp_FR.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_R_BR_TEMP);
    
    brakeTemp_FL.Temp = lib_interpolation_interpolate(&brakeTemp_FLmap, brakeTemp_FL.voltage);
    brakeTemp_FR.Temp = lib_interpolation_interpolate(&brakeTemp_FRmap, brakeTemp_FR.voltage);
}

const ModuleDesc_S brakeTemp_desc = {
    .moduleInit       = &brakeTemp_init,
    .periodic10Hz_CLK = &brakeTemp_periodic_10Hz,
};