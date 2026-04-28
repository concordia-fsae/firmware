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

#include "app_faultManager.h"
#include "app_gps.h"
#include "app_vehicleState.h"
#include "lib_simpleFilter.h"
#include <math.h>
#include "Yamcan.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define WHEEL_CIRCUMFERENCE_M 1.25679f

#define DEG_TO_RAD (0.01745329252f)
#define GRAVITY 9.81f

#define HZ_TO_RPM(hz) ((uint16_t)((hz) * 60))
#define RPM_TO_HZ(rpm) ((rpm) / 60.0f)
#define RPM_TO_MPS(hz) (RPM_TO_HZ((float32_t)hz) * WHEEL_CIRCUMFERENCE_M)
#define MOTOR_SPEED_ZERO_RPM 10
#define DRIVETRAIN_MULTIPLIER 4.6f
#define WHEEL_LOCK_DETECT_RPM 10U
#define WHEEL_LOCK_TIMEOUT_MS 500U

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#if (APP_COMPONENT_ID == FDEFS_COMPONENT_ID_VCFRONT) || (APP_COMPONENT_ID == FDEFS_COMPONENT_ID_VCREAR)
#define CANRX_MOTOR_SPEED(val) CANRX_get_signal(VEH, PM100DX_motorSpeedCritical, val)
#endif

#if FEATURE_IS_DISABLED(FEATURE_VEHICLESPEED_LEADER)
#define CANRX_VEHICLESPEED(val) CANRX_get_signal(VEH, VCFRONT_vehicleSpeed, val)
#define CANRX_ODOMETER(val) CANRX_get_signal(VEH, VCFRONT_odometer, val)
#else
#define CANRX_IMU_LON(val) CANRX_get_signal(VEH, VCPDU_lon, val)
#define CANRX_IMU_ANGLEPITCH(val) CANRX_get_signal(VEH, VCPDU_anglePitch, val)
#endif

#if APP_COMPONENT_ID == FDEFS_COMPONENT_ID_VCFRONT
#define WHEELSPEED_FAULT_DEGRADED FM_FAULT_VCFRONT_WHEELSPEEDSENSORDEGRADED
#define WHEELSPEED_FAULT_LOCKED   FM_FAULT_VCFRONT_WHEELSPEEDSENSORLOCKED
#define WHEELSPEED_FAULTS_ENABLED 1
#elif APP_COMPONENT_ID == FDEFS_COMPONENT_ID_VCREAR
#define WHEELSPEED_FAULT_DEGRADED FM_FAULT_VCREAR_WHEELSPEEDSENSORDEGRADED
#define WHEELSPEED_FAULT_LOCKED   FM_FAULT_VCREAR_WHEELSPEEDSENSORLOCKED
#define WHEELSPEED_FAULTS_ENABLED 1
#else
#define WHEELSPEED_FAULTS_ENABLED 0
#endif

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint16_t raw_rpm_wheel[WHEEL_CNT];
    uint16_t rpm_wheel[WHEEL_CNT];
    uint16_t rpm_axle[AXLE_CNT];
    bool     wheelDegraded[WHEEL_CNT];
    bool     wheelLocked[WHEEL_CNT];
    bool     axleValid[AXLE_CNT];
    float32_t vehicleSpeedLinear;

    uint32_t lastTimestampMS;

#if FEATURE_IS_ENABLED(FEATURE_VEHICLESPEED_LEADER)
    lib_simpleFilter_lpf_S lpfSpeed;
    bool odoSaved;
    bool wasValidGPS;
    uint64_t lastFrontWheelSampleBaseTick;
#endif // FEATURE_VEHICLEPEED_LEADER
} vehicle_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static vehicle_S vehicle;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static axle_E wheelToAxle(wheel_E wheel)
{
    return (wheel <= WHEEL_FR) ? AXLE_FRONT : AXLE_REAR;
}

static axle_E otherAxle(axle_E axle)
{
    return (axle == AXLE_FRONT) ? AXLE_REAR : AXLE_FRONT;
}

static wheel_E otherWheelOnAxle(wheel_E wheel)
{
    switch (wheel)
    {
        case WHEEL_FL:
            return WHEEL_FR;
        case WHEEL_FR:
            return WHEEL_FL;
        case WHEEL_RL:
            return WHEEL_RR;
        case WHEEL_RR:
        default:
            return WHEEL_RL;
    }
}

static bool isWheelUnavailable(wheel_E wheel)
{
    return vehicle.wheelDegraded[wheel] || vehicle.wheelLocked[wheel];
}

#if FEATURE_IS_ENABLED(FEATURE_VEHICLESPEED_LEADER)
static bool hasValidFrontWheelReference(void)
{
    return !isWheelUnavailable(WHEEL_FL) || !isWheelUnavailable(WHEEL_FR);
}

static uint64_t getWheelLastSampleBaseTick(wheel_E wheel)
{
    if (app_wheelSpeed_config.sensorType[wheel] != WS_SENSORTYPE_TIM_CHANNEL)
    {
        return 0U;
    }

    return HW_TIM_getLastCaptureBaseTick(app_wheelSpeed_config.config[wheel].channel_freq);
}

static bool getFreshFrontWheelReference(uint64_t* latestSampleBaseTick)
{
    uint64_t latestFrontWheelSample = 0U;

    if (!isWheelUnavailable(WHEEL_FL))
    {
        const uint64_t flSample = getWheelLastSampleBaseTick(WHEEL_FL);
        latestFrontWheelSample = flSample;
    }

    if (!isWheelUnavailable(WHEEL_FR))
    {
        const uint64_t frSample = getWheelLastSampleBaseTick(WHEEL_FR);
        if (frSample > latestFrontWheelSample)
        {
            latestFrontWheelSample = frSample;
        }
    }

    *latestSampleBaseTick = latestFrontWheelSample;

    return (latestFrontWheelSample != 0U) && (latestFrontWheelSample > vehicle.lastFrontWheelSampleBaseTick);
}
#endif

#if WHEELSPEED_FAULTS_ENABLED
static bool getMotorAxleRpm(uint16_t* rpm)
{
#if (APP_COMPONENT_ID == FDEFS_COMPONENT_ID_VCFRONT) || (APP_COMPONENT_ID == FDEFS_COMPONENT_ID_VCREAR)
    int16_t motor_rpm = 0;

    if (CANRX_MOTOR_SPEED(&motor_rpm) == CANRX_MESSAGE_VALID)
    {
        motor_rpm = (int16_t)(motor_rpm < 0 ? -motor_rpm : motor_rpm);
        *rpm = (uint16_t)(motor_rpm / DRIVETRAIN_MULTIPLIER);
        return true;
    }
#endif

    *rpm = 0U;
    return false;
}
#endif

static uint16_t getFallbackWheelRpm(wheel_E wheel)
{
    const axle_E axle = wheelToAxle(wheel);

    if (vehicle.axleValid[axle])
    {
        return vehicle.rpm_axle[axle];
    }

    if (vehicle.axleValid[otherAxle(axle)])
    {
        return vehicle.rpm_axle[otherAxle(axle)];
    }

    return 0U;
}

static uint16_t getWheelCalculationRpm(wheel_E wheel)
{
    return isWheelUnavailable(wheel) ? getFallbackWheelRpm(wheel) : vehicle.raw_rpm_wheel[wheel];
}

static void calculateWheelFaults(void)
{
#if WHEELSPEED_FAULTS_ENABLED
    bool anyDegraded = false;
    bool anyLocked = false;

    for (uint8_t i = 0U; i < WHEEL_CNT; i++)
    {
        if (app_wheelSpeed_config.sensorType[i] == WS_SENSORTYPE_TIM_CHANNEL)
        {
            uint16_t referenceRpm = 0U;
            uint16_t motorAxleRpm = 0U;

            for (uint8_t j = 0U; j < WHEEL_CNT; j++)
            {
                if ((i != j) && !isWheelUnavailable(j) && (vehicle.raw_rpm_wheel[j] > referenceRpm))
                {
                    referenceRpm = vehicle.raw_rpm_wheel[j];
                }
            }

            if ((referenceRpm == 0U) && getMotorAxleRpm(&motorAxleRpm))
            {
                referenceRpm = motorAxleRpm;
            }

            if (referenceRpm >= WHEEL_LOCK_DETECT_RPM)
            {
                vehicle.wheelLocked[i] = vehicle.raw_rpm_wheel[i] == 0U;
            }
            else
            {
                vehicle.wheelLocked[i] = false;
            }
        }

        anyDegraded |= isWheelUnavailable(i);
        anyLocked |= vehicle.wheelLocked[i];
    }

    app_faultManager_setFaultState(WHEELSPEED_FAULT_DEGRADED, anyDegraded);
    app_faultManager_setFaultState(WHEELSPEED_FAULT_LOCKED, anyLocked);
#endif
}

static void calculateAxleSpeed(void)
{
    for (uint8_t axle = 0U; axle < AXLE_CNT; axle++)
    {
        const wheel_E wheelLeft = (axle == AXLE_FRONT) ? WHEEL_FL : WHEEL_RL;
        const wheel_E wheelRight = otherWheelOnAxle(wheelLeft);
        uint16_t      sum = 0U;
        uint8_t       count = 0U;

        if (!isWheelUnavailable(wheelLeft))
        {
            sum += vehicle.raw_rpm_wheel[wheelLeft];
            count++;
        }
        if (!isWheelUnavailable(wheelRight))
        {
            sum += vehicle.raw_rpm_wheel[wheelRight];
            count++;
        }

        vehicle.axleValid[axle] = count > 0U;
        vehicle.rpm_axle[axle] = (uint16_t)((count > 0U) ? (sum / count) : 0U);
    }
}

static void calculateWheelSpeed(void)
{
    for (uint8_t i = 0U; i < WHEEL_CNT; i++)
    {
        vehicle.rpm_wheel[i] = getWheelCalculationRpm((wheel_E)i);
    }
}

static void calculateVehicleSpeed(void)
{
#if FEATURE_IS_ENABLED(FEATURE_VEHICLESPEED_LEADER)
    float32_t speed = vehicle.vehicleSpeedLinear;
    float32_t accelLon = 0.0f;
    float32_t anglePitch = 0.0f;
    int16_t motor_rpm = 0;
    const bool accelValid = (CANRX_IMU_LON(&accelLon) == CANRX_MESSAGE_VALID);
    const bool angleValid = (CANRX_IMU_ANGLEPITCH(&anglePitch) == CANRX_MESSAGE_VALID);
    const bool motorValid = (CANRX_MOTOR_SPEED(&motor_rpm) == CANRX_MESSAGE_VALID);
    const uint32_t currentTime = HW_TIM_getTimeMS();
    const float32_t delta_t = (float32_t)(currentTime - vehicle.lastTimestampMS) / 1000.0f;
    const bool validGPS = app_gps_isValid();
    const uint16_t frontAxleRpm = app_vehicleSpeed_getAxleSpeedRotational(AXLE_FRONT);
    uint64_t frontWheelSampleBaseTick = 0U;
    const bool motorInReverse = (motor_rpm < 0) && motorValid;

    if (accelValid && angleValid && (delta_t > 0.0f))
    {
        const float32_t accelAlongAxis = accelLon + (GRAVITY * sinf(anglePitch * DEG_TO_RAD));
        speed += accelAlongAxis * delta_t;
    }

    if (validGPS)
    {
        if (!vehicle.wasValidGPS)
        {
            speed = app_gps_getHeadingRef()->speedMps;
            vehicle.lpfSpeed.y = motorInReverse ? -speed : speed;
        }
        else
        {
            const float32_t tmp = app_gps_getHeadingRef()->speedMps;
            speed = lib_simpleFilter_lpf_step(&vehicle.lpfSpeed, motorInReverse ? -tmp : tmp);
        }
    }
    if (hasValidFrontWheelReference() && getFreshFrontWheelReference(&frontWheelSampleBaseTick))
    {
        const float32_t tmp = RPM_TO_MPS(frontAxleRpm);
        speed = lib_simpleFilter_lpf_step(&vehicle.lpfSpeed, motorInReverse ? -tmp : tmp);
        vehicle.lastFrontWheelSampleBaseTick = frontWheelSampleBaseTick;
    }

    if (motorValid)
    {
        motor_rpm = (int16_t)(motor_rpm < 0 ? -motor_rpm : motor_rpm);
        if (motor_rpm < MOTOR_SPEED_ZERO_RPM)
        {
            speed = lib_simpleFilter_lpf_step(&vehicle.lpfSpeed, 0.0f);
        }
    }

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
    app_faultManager_setFaultState(FM_FAULT_VCFRONT_VEHICLESPEEDDEGRADED, !validGPS || !hasValidFrontWheelReference());
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

uint16_t app_vehicleSpeed_getWheelSpeedRawRotational(wheel_E wheel)
{
    return vehicle.raw_rpm_wheel[wheel];
}

uint16_t app_vehicleSpeed_getWheelSpeedRotational(wheel_E wheel)
{
    return vehicle.rpm_wheel[wheel];
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
            vehicle.raw_rpm_wheel[i] = HZ_TO_RPM(HW_TIM_getFreq(app_wheelSpeed_config.config[i].channel_freq));
            vehicle.wheelDegraded[i] = false;
            vehicle.wheelLocked[i] = false;
        }
        else
        {
            if (app_wheelSpeed_config.sensorType[i] == WS_SENSORTYPE_CAN_RPM)
            {
                if (app_wheelSpeed_config.config[i].rpm(&vehicle.raw_rpm_wheel[i]) != CANRX_MESSAGE_VALID)
                {
                    vehicle.raw_rpm_wheel[i] = 0U;
                    vehicle.wheelDegraded[i] = true;
                    vehicle.wheelLocked[i] = false;
                }
                else
                {
                    vehicle.wheelDegraded[i] = false;
                    vehicle.wheelLocked[i] = false;
                }
            }
            else
            {
                vehicle.raw_rpm_wheel[i] = 0U;
                vehicle.wheelDegraded[i] = false;
                vehicle.wheelLocked[i] = false;
            }
        }
    }

    calculateWheelFaults();
    calculateAxleSpeed();
    calculateWheelSpeed();
    calculateVehicleSpeed();
}

const ModuleDesc_S app_vehicleSpeed_desc = {
    .moduleInit = &app_vehicleSpeed_init,
    .periodic100Hz_CLK = &app_vehicleSpeed_periodic_100Hz,
};
