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

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define WHEEL_DIAMETER_M 0.4064f
#define HZ_TO_RPM(hz) ((hz) * 60.0f)
#define HZ_TO_MPS(hz) ((hz) * WHEEL_DIAMETER_M)

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t hz[WHEELSPEED_SENSOR_CNT];
} wheelSpeed_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static wheelSpeed_S wheels;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

uint16_t wheelSpeed_getSpeedRotational(wheelSpeed_Sensor_E wheel)
{
    return (uint16_t)HZ_TO_RPM(wheels.hz[wheel]);
}

float32_t wheelSpeed_getSpeedLinear(wheelSpeed_Sensor_E wheel)
{
    return HZ_TO_MPS(wheels.hz[wheel]);
}

static void wheelSpeed_init(void)
{
    memset(&wheels, 0x00U, sizeof(wheels));
}

static void wheelSpeed_periodic_100Hz(void)
{
    wheels.hz[WHEELSPEED_SENSOR_R] = HW_TIM_getFreq(HW_TIM_CHANNEL_WS_R);
    wheels.hz[WHEELSPEED_SENSOR_L] = HW_TIM_getFreq(HW_TIM_CHANNEL_WS_L);
}

const ModuleDesc_S wheelSpeed_desc = {
    .moduleInit = &wheelSpeed_init,
    .periodic100Hz_CLK = &wheelSpeed_periodic_100Hz,
};
