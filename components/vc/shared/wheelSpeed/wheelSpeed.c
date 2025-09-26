/**
 * @file wheelSpeed.c
 * @brief Module source for steering angle sensor
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/
//#include "MessageUnpack_generated.h"
#include "HW_tim.h"
#include "lib_utility.h"
#include "ModuleDesc.h"
#include "string.h"
#include "wheelSpeed.h"
#include <math.h>


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define WHEEL_DIAMETER_M    0.4064f
#define HZ_TO_RPM(hz)       ((hz) * 60.0f)
#define RPM_TO_HZ(rpm)      ((rpm) / 60.0f)
#define HZ_TO_MPS(hz)       ((hz) * WHEEL_DIAMETER_M)
#define MPS_TO_HZ(mps)      ((mps) / WHEEL_DIAMETER_M)

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t hz[WHEEL_CNT];
} wheelSpeed_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/
static struct
{
    float32_t    slipRatio;
    wheelSpeed_S wheels;
} ws_data;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

uint16_t wheelSpeed_getSpeedRotational(wheel_E wheel)
{
    float32_t rpm = 0.0f;

    switch (wheelSpeed_config.sensorType[wheel])
    {
        case WS_SENSORTYPE_CAN_RPM:
        {
            float32_t temp = 0.0f;
            if (wheelSpeed_config.config[wheel].rpm(&temp) == CANRX_MESSAGE_VALID)
            {
                rpm = temp;
            }
        }
        break;

        case WS_SENSORTYPE_CAN_MPS:
        {
            float32_t temp = 0.0f;
            if (wheelSpeed_config.config[wheel].mps(&temp) == CANRX_MESSAGE_VALID)
            {
                rpm = HZ_TO_RPM(MPS_TO_HZ(temp));
            }
        }
        break;

        default:
            rpm = HZ_TO_MPS(ws_data.wheels.hz[wheel]);
            break;
    }

    return (uint16_t)rpm;
}

float32_t wheelSpeed_getSpeedLinear(wheel_E wheel)
{
    float32_t mps = 0.0f;

    switch (wheelSpeed_config.sensorType[wheel])
    {
        case WS_SENSORTYPE_CAN_RPM:
        {
            float32_t temp = 0.0f;
            if (wheelSpeed_config.config[wheel].rpm(&temp) == CANRX_MESSAGE_VALID)
            {
                mps = HZ_TO_MPS(RPM_TO_HZ(temp));
            }
        }
        break;

        case WS_SENSORTYPE_CAN_MPS:
        {
            float32_t temp = 0.0f;
            if (wheelSpeed_config.config[wheel].mps(&temp) == CANRX_MESSAGE_VALID)
            {
                mps = temp;
            }
        }
        break;

        default:
            mps = HZ_TO_MPS(ws_data.wheels.hz[wheel]);
            break;
    }

    return mps;
}

float32_t wheelSpeed_getSlipRatio(void)
{
    return ws_data.slipRatio;
}

static void calcSlipRatio_100Hz(void)
{
    float32_t FR_wheelSpeed=0.0f;
    float32_t RR_wheelSpeed=0.0f;
    float32_t FL_wheelSpeed=0.0f;
    float32_t RL_wheelSpeed=0.0f;
    
   

    CANRX_VEH_get_VCREAR_wheelSpeedRotationalRL(&RL_wheelSpeed);
    CANRX_VEH_get_VCREAR_wheelSpeedRotationalRR(&RR_wheelSpeed);
    CANRX_VEH_get_VCFRONT_wheelSpeedRotationalFR(&FR_wheelSpeed);
    CANRX_VEH_get_VCFRONT_wheelSpeedRotationalFL(&FL_wheelSpeed);
   

    if (fabs(FR_wheelSpeed) > 1e-6f)
    {
        ws_data.slipRatio = ((RR_wheelSpeed - FR_wheelSpeed) / FR_wheelSpeed) * 100;
    }
    else
    {
        ws_data.slipRatio = 0.0f;
    }
}

static void wheelSpeed_init(void)
{
    memset(&ws_data.wheels, 0x00U, sizeof(ws_data.wheels));
}

static void wheelSpeed_periodic_100Hz(void)
{
    for (uint8_t i = 0x00U; i < WHEEL_CNT; i++)
    {
        if (wheelSpeed_config.sensorType[i] == WS_SENSORTYPE_TIM_CHANNEL)
        {
            ws_data.wheels.hz[i] = HW_TIM_getFreq(wheelSpeed_config.config[i].channel_freq);
        }
    }
    calcSlipRatio_100Hz();
}

const ModuleDesc_S wheelSpeed_desc = {
    .moduleInit        = &wheelSpeed_init,
    .periodic100Hz_CLK = &wheelSpeed_periodic_100Hz,
};
