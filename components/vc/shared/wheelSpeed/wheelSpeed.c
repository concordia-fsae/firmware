/**
 * @file wheelSpeed.c
 * @brief Module source for steering angle sensor
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "wheelSpeed.h"
#include "ModuleDesc.h"
#include "string.h"
#include "HW_tim.h"
#include "wheel.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define WHEEL_DIAMETER_M 0.4064f
#define HZ_TO_RPM(hz) ((uint16_t)((hz) * 60))
#define RPM_TO_HZ(rpm) ((rpm) / 60.0f)
#define RPM_TO_MPS(hz) (RPM_TO_HZ(hz) * WHEEL_DIAMETER_M)

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint16_t rpm_wheel[WHEEL_CNT];
    uint16_t rpm_axle[AXLE_CNT];
} wheelSpeed_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static wheelSpeed_S wheels;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void calculateAxleSpeed(void)
{
    wheels.rpm_axle[AXLE_FRONT] = (uint16_t)(wheels.rpm_wheel[WHEEL_FL] + wheels.rpm_wheel[WHEEL_FR]) / 2;
    wheels.rpm_axle[AXLE_REAR] = (uint16_t)(wheels.rpm_wheel[WHEEL_RL] + wheels.rpm_wheel[WHEEL_RR]) / 2;
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

uint16_t wheelSpeed_getAxleRPM(axle_E axle)
{
    return wheels.rpm_axle[axle];
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
            rpm = wheels.rpm_wheel[wheel];
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
                    mps = RPM_TO_MPS(temp);
                }
            }
            break;
        default:
            mps = RPM_TO_MPS(wheels.rpm_wheel[wheel]);
            break;
    }

    return mps;
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
            wheels.rpm_wheel[i] = HZ_TO_RPM(HW_TIM_getFreq(wheelSpeed_config.config[i].channel_freq));
        }
        else
        {
            wheels.rpm_wheel[i] = wheelSpeed_getSpeedRotational(i);
        }
    }

    calculateAxleSpeed();
}

const ModuleDesc_S wheelSpeed_desc = {
    .moduleInit = &wheelSpeed_init,
    .periodic100Hz_CLK = &wheelSpeed_periodic_100Hz,
};
