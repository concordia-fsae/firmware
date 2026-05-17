/**
 * @file brakeTemp.c
 * @brief Module source for brakeTemp sensors
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "brakeTemp.h"
#include "drv_inputAD.h"
#include "lib_interpolation.h"
#include "lib_utility.h"
#include "ModuleDesc.h"
#include <string.h>

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/
typedef struct
{
    float32_t voltage;
    float32_t temperature;
} brakeTempData_S;

typedef struct
{
    brakeTempData_S data[BRAKETEMP_COUNT];
} brakeTemp_S;
/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static brakeTemp_S                 brakeTemp;

static lib_interpolation_point_S   brakeTemp_MapPoints[] = {
    {
        .x = 0.3f,    // voltage (V) @ 0°C
        .y = 0.0f,    // temperature (°C)
    },
    {
        .x = 2.7f,    // voltage (V) @ 800°C
        .y = 800.0f,
    },
};

static lib_interpolation_mapping_S brakeTemp_map = {
    .points         = brakeTemp_MapPoints,
    .number_points  = COUNTOF(brakeTemp_MapPoints),
    .saturate_left  = true,
    .saturate_right = true,
};

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t brakeTemp_getTemperature(brakeTemp_E temp)
{
    return brakeTemp.data[temp].temperature;
}

float32_t brakeTemp_getVoltage(brakeTemp_E temp)
{
    return brakeTemp.data[temp].voltage;
}

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void brakeTemp_init(void)
{
    memset(&brakeTemp, 0x00U, sizeof(brakeTemp));
    lib_interpolation_init(&brakeTemp_map, 0.0f);
}

static void brakeTemp_periodic_10Hz(void)
{
    brakeTemp.data[BRAKETEMP_LEFT].voltage      = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_L_BR_TEMP);
    brakeTemp.data[BRAKETEMP_RIGHT].voltage     = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_R_BR_TEMP);
    brakeTemp.data[BRAKETEMP_LEFT].temperature  = lib_interpolation_interpolate(&brakeTemp_map, brakeTemp.data[BRAKETEMP_LEFT].voltage);
    brakeTemp.data[BRAKETEMP_RIGHT].temperature = lib_interpolation_interpolate(&brakeTemp_map, brakeTemp.data[BRAKETEMP_RIGHT].voltage);
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S brakeTemp_desc = {
    .moduleInit       = &brakeTemp_init,
    .periodic10Hz_CLK = &brakeTemp_periodic_10Hz,
};

