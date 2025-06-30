/**
 * @file steeringAngle.c
 * @brief Module source for steering angle sensor
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "steeringAngle.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "drv_inputAD_componentSpecific.h"
#include "drv_inputAD.h"
#include "MessageUnpack_generated.h"
#include "lib_interpolation.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    float32_t voltage;
    float32_t angle;
} steeringAngle_data;

static lib_interpolation_point_S steering_angle[] = {
    {
        .x = 1.414f, // sensor reference voltage
        .y = -102.0f, // left turned degrees
    },
    {
        .x = 0.894f, // voltage
        .y = 0.0f, // center degrees
    },
    {
        .x = 0.345f, // voltage
        .y = 102.0f, // right turned degrees
    },  
};

static lib_interpolation_mapping_S steering_map = {
    .points = (lib_interpolation_point_S*)&steering_angle,
    .number_points = COUNTOF(steering_angle),
    .saturate_left = false,
    .saturate_right = true,
};

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t steeringAngle_getSteeringAngle(void) 
{
    return steeringAngle_data.angle;
}

float32_t steeringAngle_getSteeringVoltage(void){
    return steeringAngle_data.voltage;
}
/**
 * @brief Initializes steeringAngle_data struct and interpolation init
 */
static void steeringAngle_init(void)
{
    memset(&steeringAngle_data, 0x00U, sizeof(steeringAngle_data)); 
    lib_interpolation_init(&steering_map, 0.0f);
}

/**
 * @brief 100ms (10Hz) Function that periodically calls drv_inputAD_getAnalogVoltage
 * @note Uses lib_interpolation library to map and interpolate values, takes into account edge cases with saturation
 * @note Has a voltage divider compensation of 1.681 for 3v to 5v
 */
static void steeringAngle_periodic_10Hz(void)
{
    steeringAngle_data.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_STR_ANGLE);
    steeringAngle_data.angle = (lib_interpolation_interpolate(&steering_map, steeringAngle_data.voltage));
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S steeringAngle_desc = {
    .moduleInit = &steeringAngle_init,
    .periodic10Hz_CLK = &steeringAngle_periodic_10Hz,
};
