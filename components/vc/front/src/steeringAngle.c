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
#include "app_faultManager.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define DEG90_V 0.78f
#define OC_SC_V_MARGIN 0.250f
#define MAX_SENSOR_VOLTAGE 3.0f

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    float32_t voltage;
    float32_t angle;
} steeringAngle_data;

// @note Interpolate voltage from zero-angle calibration voltage
static lib_interpolation_point_S steering_angle[] = {
    {
        .x = - DEG90_V, // voltage
        .y = 90.0f, // right turned degrees
    },
    {
        .x = DEG90_V, // sensor reference voltage
        .y = - 90.0f, // left turned degrees
    },
};

static lib_interpolation_mapping_S steering_map = {
    .points = (lib_interpolation_point_S*)&steering_angle,
    .number_points = COUNTOF(steering_angle),
    .saturate_left = false,
    .saturate_right = false,
};

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

nvm_steeringCalibration_S steeringCalibration_data;

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
    bool faulted = false;
    CAN_digitalStatus_E tmp = CAN_DIGITALSTATUS_SNA;
    const bool calibrate = (CANRX_get_signal(VEH, SWS_requestCalibSteerAngle, &tmp) == CANRX_MESSAGE_VALID) &&
                           (tmp == CAN_DIGITALSTATUS_ON);

    steeringAngle_data.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_STR_ANGLE);

    if (calibrate)
    {
        steeringCalibration_data.zero = steeringAngle_data.voltage;
        lib_nvm_requestWrite(NVM_ENTRYID_STEERINGCALIBRATION);
    }

    if ((steeringAngle_data.voltage < OC_SC_V_MARGIN) ||
        (steeringAngle_data.voltage > (MAX_SENSOR_VOLTAGE - OC_SC_V_MARGIN)))
    {
        faulted = true;
        steeringAngle_data.angle = 0.0f;
    }
    else
    {
        steeringAngle_data.angle = (lib_interpolation_interpolate(&steering_map, steeringAngle_data.voltage - steeringCalibration_data.zero));
    }

    app_faultManager_setFaultState(FM_FAULT_VCFRONT_STEERINGSENSORFAULT, faulted);
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S steeringAngle_desc = {
    .moduleInit = &steeringAngle_init,
    .periodic10Hz_CLK = &steeringAngle_periodic_10Hz,
};
