/**
* @file vd.c
* @brief Module source for the vehicle dynamics models
*/

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "vd.h"
#include "steeringAngle.h"
#include "lib_interpolation.h"
#include "lib_utility.h"

#include <math.h>

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

// Tire model evaluated static under 900N loading
#define MAX_TIRE_TRACTION_N 1875.0f
#define WHEELBASE_M 1.545f

#define CONVERT_PERTIRE_N_TO_G(n) (n / (VEHICLE_MASS_KG / (2.0f * 9.81f)))

#define TURNING_THRESHOLD_DEG 2.5f

// Assumes small angle approx
#define STW_TO_WHEEL_ROTATION_SCALAR 0.2147f

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static lib_interpolation_point_S perTireMaxTraction[] = {
    {
        .x = 0.25, // lon g
        .y = MAX_TIRE_TRACTION_N, // max lat force
    },
    {
        .x = CONVERT_PERTIRE_N_TO_G(1250.0f), // lon g
        .y = 1750.0f, // max lat force
    },
    {
        .x = CONVERT_PERTIRE_N_TO_G(1500.0f), // lon g
        .y = 1625.0f, // max lat force
    },
    {
        .x = CONVERT_PERTIRE_N_TO_G(2100.0f), // lon g
        .y = 250.0f, // max lat force
    },
};
static lib_interpolation_mapping_S mapTraction = {
    .points = (lib_interpolation_point_S*)&perTireMaxTraction,
    .number_points = COUNTOF(perTireMaxTraction),
    .saturate_left = true,
    .saturate_right = true,
};

static struct
{
    float32_t currentLatAccel;
    float32_t currentLonAccel;
    float32_t maxLonTireForce;
    float32_t driverIntendedRadius;

    bool driverTurning;
} vdData;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void setIntendedRadius(float32_t steeringAngle)
{
    const float32_t steeringRad = steeringAngle * DEG_TO_RAD;
    const float32_t wheelRad = steeringRad * STW_TO_WHEEL_ROTATION_SCALAR;
    const bool driverTurning = fabsf(steeringAngle) > TURNING_THRESHOLD_DEG;
    vdData.driverTurning = driverTurning;

    if (driverTurning)
    {
        const float32_t a = powf(WHEELBASE_M / atanf(wheelRad), 2);
        const float32_t b = powf(WHEELBASE_M / 2, 2);
        vdData.driverIntendedRadius = sqrtf(a + b);
    }
    else
    {
        vdData.driverIntendedRadius = STRAIGHTLINE_RADIUS;
    }
}

static void evaluateTireModel(bool accelValid, float32_t lonAccel, float32_t latAccel)
{
    if (accelValid)
    {
        vdData.currentLatAccel = latAccel;
        vdData.currentLonAccel = lonAccel;
        vdData.maxLonTireForce = lib_interpolation_interpolate(&mapTraction, fabsf(latAccel));
    }
    else
    {
        vdData.currentLatAccel = 0.0f;
        vdData.currentLonAccel = 0.0f;
        vdData.maxLonTireForce = MAX_TIRE_TRACTION_N;
    }
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t vd_getIntendedRadius(void)
{
    return vdData.driverIntendedRadius;
}

float32_t vd_getMaxLonTireForce(wheel_E wheel)
{
    UNUSED(wheel);
    return vdData.maxLonTireForce;
}

static void init(void)
{
    memset(&vdData, 0x00U, sizeof(vdData));
}

static void periodic_100Hz(void)
{
    float32_t latAccel, lonAccel;
    const bool accelValid = (CANRX_get_signal(VEH, VCPDU_lat, &latAccel) == CANRX_MESSAGE_VALID);
    (void)CANRX_get_signal(VEH, VCPDU_lon, &lonAccel);
    const float32_t steeringAngle = steeringAngle_getSteeringAngle();

    setIntendedRadius(steeringAngle);
    evaluateTireModel(accelValid, lonAccel, latAccel);
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S vd_desc = {
    .moduleInit = &init,
    .periodic100Hz_CLK = &periodic_100Hz,
};
