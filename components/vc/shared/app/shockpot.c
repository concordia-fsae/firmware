/**
 * @file shockpot.c
 * @brief Module source for steering angle sensor
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/
#include "lib_utility.h"
#include "shockpot.h"
#include "ModuleDesc.h"
#include "drv_inputAD.h"
#include "lib_interpolation.h"
#include <string.h>

typedef struct {
    float32_t voltage;
    float32_t displacement;
} shockpotData_S;

typedef struct {
    shockpotData_S data[SHOCKPOT_COUNT];
} shockpot_S;

static shockpot_S shockpot;

static lib_interpolation_point_S shockpot_MapPoints[] = {
    {
        .x = 0.3f, // voltage
        .y = 75.0f, 
    },
    {
        .x = 2.7f, // sensor reference voltage
        .y = 0.0f, 
    },
};

float32_t shockpot_getDisplacement(shockpot_E pot)
{
    return shockpot.data[pot].displacement;
}

float32_t shockpot_getVoltage(shockpot_E pot)
{
    return shockpot.data[pot].voltage;
}

static lib_interpolation_mapping_S shockpot_map = {
    .points = shockpot_MapPoints,
    .number_points = COUNTOF(shockpot_MapPoints),
    .saturate_left = true,
    .saturate_right = true,
};

static void shockpot_init(void)
{
    memset(&shockpot, 0x00U, sizeof(shockpot));
    lib_interpolation_init(&shockpot_map, 0.0f);
}

static void shockpot_periodic_100Hz(void)
{
    shockpot.data[SHOCKPOT_LEFT].voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_L_SHK_DISP);
    shockpot.data[SHOCKPOT_RIGHT].voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_R_SHK_DISP);
    shockpot.data[SHOCKPOT_LEFT].displacement = lib_interpolation_interpolate(&shockpot_map, shockpot.data[SHOCKPOT_LEFT].voltage);
    shockpot.data[SHOCKPOT_RIGHT].displacement = lib_interpolation_interpolate(&shockpot_map, shockpot.data[SHOCKPOT_RIGHT].voltage);

}

const ModuleDesc_S shockpot_desc = {
    .moduleInit = &shockpot_init,
    .periodic100Hz_CLK = &shockpot_periodic_100Hz,
};
