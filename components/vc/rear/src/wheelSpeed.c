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

#define WHEEL_DIAMETER_M 0.4064

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint16_t rpm[WHEELSPEED_SENSOR_CNT];
    float32_t mps[WHEELSPEED_SENSOR_CNT]; // Meter per second
} wheelSpeed_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static wheelSpeed_S wheels;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

uint16_t wheelSpeed_getRpm(wheelSpeed_Sensor_E wheel)
{
    return wheels.rpm[wheel];
}

float32_t wheelSpeed_getMps(wheelSpeed_Sensor_E wheel)
{
    return wheels.mps[wheel];
}

static void wheelSpeed_init(void)
{
    memset(&wheels, 0x00U, sizeof(wheels));
}

static void wheelSpeed_periodic_10Hz(void)
{
    wheels.rpm[WHEELSPEED_SENSOR_R] = (uint16_t)HW_TIM4_getFreqCH3() * 60;
    wheels.rpm[WHEELSPEED_SENSOR_L] = (uint16_t)HW_TIM4_getFreqCH4() * 60;

    for (uint8_t i = 0; i < WHEELSPEED_SENSOR_CNT; i++)
    {
        wheels.mps[i] = (float32_t)(((float32_t)wheels.rpm[i] / 60.0f) * WHEEL_DIAMETER_M);
    }
}

const ModuleDesc_S wheelSpeed_desc = {
    .moduleInit = &wheelSpeed_init,
    .periodic10Hz_CLK = &wheelSpeed_periodic_10Hz,
};
