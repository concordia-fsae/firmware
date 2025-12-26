/**
 * @file wheelSpeed.c
 * @brief Module source for steering angle sensor
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "wheelSpeed.h"
#include "MessageUnpack_generated.h"
#include "HW_tim.h"
#include "lib_utility.h"
#include "ModuleDesc.h"
#include "string.h"
#include "HW_tim.h"
#include "wheel.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define WHEEL_DIAMETER_M 0.4064f
#define HZ_TO_RPM(hz) ((uint16_t)((hz) * 60))
#define RPM_TO_HZ(rpm) ((rpm) / 60)
#define HZ_TO_MPS(hz) ((hz) * WHEEL_DIAMETER_M)
#define MPS_TO_HZ(mps) ((mps) / WHEEL_DIAMETER_M)

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t slipRatio;
    float32_t hz[WHEEL_CNT];
    float32_t hz_axle[AXLE_CNT];
    float32_t vehicle_mps;
} wheelSpeed_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static wheelSpeed_S wheels;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

uint16_t wheelSpeed_getAxleRPM(axle_E axle)
{
    return HZ_TO_RPM(wheels.hz_axle[axle]);
}

uint16_t wheelSpeed_getSpeedRotational(wheel_E wheel)
{
    uint16_t rpm = 0.0f;

    switch (wheelSpeed_config.sensorType[wheel])
    {
        case WS_SENSORTYPE_CAN_RPM:
            {
                uint16_t temp = 0.0f;
                if (wheelSpeed_config.config[wheel].rpm(&temp) == CANRX_MESSAGE_VALID)
                {
                    rpm = temp;
                }
            }
            break;
        default:
            rpm = HZ_TO_RPM(wheels.hz[wheel]);
            break;
    }

    return rpm;
}

float32_t wheelSpeed_getSpeedLinear(wheel_E wheel)
{
    float32_t mps = 0.0f;

    switch (wheelSpeed_config.sensorType[wheel])
    {
        case WS_SENSORTYPE_CAN_RPM:
            {
                uint16_t temp = 0;
                if (wheelSpeed_config.config[wheel].rpm(&temp) == CANRX_MESSAGE_VALID)
                {
                    mps = HZ_TO_MPS(RPM_TO_HZ(temp));
                }
            }
            break;
        default:
            mps = HZ_TO_MPS(wheels.hz[wheel]);
            break;
    }

    return mps;
}

float32_t wheelSpeed_getVehicleSpeed(void)
{
    return wheels.vehicle_mps;
}

float32_t wheelSpeed_getSlipRatio(void)
{
    return wheels.slipRatio;
}

static float32_t calcVehicleSpeed(void)
{
    return (wheelSpeed_getSpeedLinear(WHEEL_FL) + wheelSpeed_getSpeedLinear(WHEEL_FR)) / 2;
}

static void calcSlipRatio(void)
{
    const float32_t rear = (wheelSpeed_getSpeedLinear(WHEEL_RL) + wheelSpeed_getSpeedLinear(WHEEL_RR)) / 2;

    if (fabs(wheels.vehicle_mps) > 1e-6f)
    {
        wheels.slipRatio = ((wheels.vehicle_mps - rear) / wheels.vehicle_mps) * 100;
    }
    else
    {
        wheels.slipRatio = 0.0f;
    }
}

static void wheelSpeed_init(void)
{
    memset(&wheels, 0x00U, sizeof(wheels));
}

static void wheelSpeed_periodic_100Hz(void)
{
    for (uint8_t i = 0x00U; i < WHEEL_CNT; i++)
    {
        if (wheelSpeed_config.sensorType[i] == WS_SENSORTYPE_TIM_CHANNEL)
        {
            wheels.hz[i] = HW_TIM_getFreq(wheelSpeed_config.config[i].channel_freq);
        }
    }

    wheels.vehicle_mps = calcVehicleSpeed();
    calcSlipRatio();
}

const ModuleDesc_S wheelSpeed_desc = {
    .moduleInit = &wheelSpeed_init,
    .periodic100Hz_CLK = &wheelSpeed_periodic_100Hz,
};
