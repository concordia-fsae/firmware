/**
 * @file app_vehicleSpeed.c
 * @brief Module source for steering angle sensor
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "app_vehicleSpeed.h"
#include "ModuleDesc.h"
#include "string.h"
#include "HW_tim.h"
#include "MessageUnpack_generated.h"
#include "lib_simpleFilter.h"
#include <math.h>

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define WHEEL_CIRCUMFERENCE_M 1.25679f

#define HZ_TO_RPM(hz) ((uint16_t)((hz) * 60))
#define RPM_TO_HZ(rpm) ((rpm) / 60.0f)
#define RPM_TO_MPS(hz) (RPM_TO_HZ((float32_t)hz) * WHEEL_CIRCUMFERENCE_M)

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#if FEATURE_IS_DISABLED(FEATURE_VEHICLESPEED_LEADER)
#define CANRX_VEHICLESPEED(val) CANRX_get_signal(VEH, VCFRONT_vehicleSpeed, val)
#define CANRX_ODOMETER(val) CANRX_get_signal(VEH, VCFRONT_odometer, val)
#endif

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint16_t rpm_wheel[WHEEL_CNT];
    uint16_t rpm_axle[AXLE_CNT];
    float32_t vehicleSpeedLinear;

#if FEATURE_IS_ENABLED(FEATURE_VEHICLESPEED_LEADER)
    uint32_t lastTimestampMS;
    lib_simpleFilter_lpf_S lpfSpeed;
    bool odoSaved;
    bool wasValidGPS;
#endif // FEATURE_VEHICLEPEED_LEADER
} vehicle_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static vehicle_S vehicle;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void calculateAxleSpeed(void)
{
    vehicle.rpm_axle[AXLE_FRONT] = (uint16_t)(vehicle.rpm_wheel[WHEEL_FL] + vehicle.rpm_wheel[WHEEL_FR]) / 2;
    vehicle.rpm_axle[AXLE_REAR] = (uint16_t)(vehicle.rpm_wheel[WHEEL_RL] + vehicle.rpm_wheel[WHEEL_RR]) / 2;
}

static void calculateVehicleSpeed(void)
{
#if FEATURE_IS_ENABLED(FEATURE_VEHICLESPEED_LEADER)
    const uint32_t currentTime = HW_TIM_getTimeMS();
    const float32_t delta_t = (float32_t)(currentTime - vehicle.lastTimestampMS) / 1000.0f;
    const bool validGPS = app_gps_isValid();
    float32_t speed = app_gps_getHeadingRef()->speedMps;

    if (!vehicle.wasValidGPS && validGPS)
    {
        vehicle.lpfSpeed.y = speed;
    }
    else if (!validGPS)
    {
        // TODO: Handle degraded wheel speed sensors
        speed = RPM_TO_MPS(app_vehicleSpeed_getAxleSpeedRotational(AXLE_FRONT));
    }

    speed = lib_simpleFilter_lpf_step(&vehicle.lpfSpeed, speed);
    vehicle.vehicleSpeedLinear = speed;

    if (app_vehicleState_getState() == VEHICLESTATE_TS_RUN)
    {
        vehicle.odoSaved = false;
        odometer_data.km += (fabsf(vehicle.vehicleSpeedLinear) * delta_t) / 1000.0f;
    }
    else if (!vehicle.odoSaved)
    {
        lib_nvm_requestWrite(NVM_ENTRYID_ODOMETER);
    }

    vehicle.lastTimestampMS = currentTime;
    vehicle.wasValidGPS = validGPS;
    app_faultManager_setFaultState(FM_FAULT_VCFRONT_VEHICLESPEEDDEGRADED, !validGPS);
#else // FEATURE_VEHICLEPEED_LEADER
    float32_t tmp = 0.0f;
    const bool valid = CANRX_VEHICLESPEED(&tmp) == CANRX_MESSAGE_VALID;
    vehicle.vehicleSpeedLinear = valid ? tmp : 0.0f;
#endif // !FEATURE_VEHICLEPEED_LEADER
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

uint16_t app_vehicleSpeed_getAxleSpeedRotational(axle_E axle)
{
    return vehicle.rpm_axle[axle];
}

uint16_t app_vehicleSpeed_getWheelSpeedRotational(wheel_E wheel)
{
    uint16_t rpm = 0.0f;

    switch (app_wheelSpeed_config.sensorType[wheel])
    {
        case WS_SENSORTYPE_CAN_RPM:
            {
                uint16_t temp = 0.0f;
                if (app_wheelSpeed_config.config[wheel].rpm(&temp) == CANRX_MESSAGE_VALID)
                {
                    rpm = temp;
                }
            }
            break;
        default:
            rpm = vehicle.rpm_wheel[wheel];
            break;
    }

    return rpm;
}

float32_t app_vehicleSpeed_getWheelSpeedLinear(wheel_E wheel)
{
    return RPM_TO_MPS(app_vehicleSpeed_getWheelSpeedRotational(wheel));
}

float32_t app_vehicleSpeed_getVehicleSpeed(void)
{
    return vehicle.vehicleSpeedLinear;
}

float32_t app_vehicleSpeed_getTireSlip(wheel_E wheel)
{
    const float32_t vel_tire = app_vehicleSpeed_getWheelSpeedLinear(wheel);
    const float32_t vel_veh = app_vehicleSpeed_getVehicleSpeed();

    return (vel_tire - vel_veh) / vel_veh;
}

float32_t app_vehicleSpeed_getAxleSlip(axle_E axle)
{
    const float32_t vel_axle = RPM_TO_MPS(app_vehicleSpeed_getAxleSpeedRotational(axle));
    const float32_t vel_veh = app_vehicleSpeed_getVehicleSpeed();

    return (vel_axle - vel_veh) / vel_veh;
}

#if FEATURE_IS_ENABLED(FEATURE_VEHICLESPEED_LEADER) || FEATURE_IS_ENABLED(FEATURE_VEHICLESPEED_USEODOMETER)
float32_t app_vehicleSpeed_getOdometer(void)
{
#if FEATURE_IS_ENABLED(FEATURE_VEHICLESPEED_LEADER)
    return odometer_data.km;
#else
    float32_t tmp = 0.0f;
    const bool valid = CANRX_ODOMETER(&tmp) == CANRX_MESSAGE_VALID;

    return valid ? tmp : 0.0f;
#endif
}
#endif

static void app_vehicleSpeed_init(void)
{
    memset(&vehicle, 0x00U, sizeof(vehicle));

#if FEATURE_IS_ENABLED(FEATURE_VEHICLESPEED_LEADER)
    lib_simpleFilter_lpf_calcSmoothingFactor(&vehicle.lpfSpeed, 100.0f, 0.01f);
    vehicle.lastTimestampMS = HW_TIM_getTimeMS();
    vehicle.odoSaved = true;
#endif
}

static void app_vehicleSpeed_periodic_100Hz(void)
{
    for (uint8_t i = 0x00U; i < WHEEL_CNT; i++)
    {
        if (app_wheelSpeed_config.sensorType[i] == WS_SENSORTYPE_TIM_CHANNEL)
        {
            vehicle.rpm_wheel[i] = HZ_TO_RPM(HW_TIM_getFreq(app_wheelSpeed_config.config[i].channel_freq));
        }
        else
        {
            vehicle.rpm_wheel[i] = app_vehicleSpeed_getWheelSpeedRotational(i);
        }
    }

    calculateAxleSpeed();
    calculateVehicleSpeed();
}

const ModuleDesc_S app_vehicleSpeed_desc = {
    .moduleInit = &app_vehicleSpeed_init,
    .periodic100Hz_CLK = &app_vehicleSpeed_periodic_100Hz,
};
