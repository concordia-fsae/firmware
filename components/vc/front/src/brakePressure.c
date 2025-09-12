/**
 * @file brakePressure.c
 * @brief Module source for brake pressure sensor
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "brakePressure.h"
#include "drv_inputAD.h"
#include "drv_inputAD_componentSpecific.h"
#include "MessageUnpack_generated.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "lib_utility.h"
#include <math.h>

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    float32_t voltage;
    float32_t pressure;
    float32_t bias;
    float32_t pressureRear;
} brakePressure_data;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t brakePressure_getBrakePressure(void)
{
    return brakePressure_data.pressure;
}

float32_t brakePressure_getBrakeBias(void)
{
    return brakePressure_data.bias;
}

static void brakePressure_init(void)
{
    memset(&brakePressure_data, 0x00U, sizeof(brakePressure_data));
}

static void calcBrakeBias_100Hz(void)
{
    CANRX_VEH_get_VCREAR_brakePressure(&brakePressure_data.pressureRear);
    const float denom = brakePressure_data.pressure + brakePressure_data.pressureRear;

    if (denom > 1e-6f)
    {
        brakePressure_data.bias = (brakePressure_data.pressure / denom) * 100;
    }
    else 
    {
        brakePressure_data.bias = 0.0f;
    }

    brakePressure_data.bias = SATURATE(0,brakePressure_data.bias,100);
}

static void brakePressure_periodic_100Hz(void)
{
    brakePressure_data.voltage = 1.681f * drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_BR_PR);
    /** Voltage division compensation: 1/(681/1k) = 1.681    */
    if (brakePressure_data.voltage <= 0.5f)
    {
        brakePressure_data.pressure = 0.0f;
    }
    else if (brakePressure_data.voltage >= 4.5f)
    {
        brakePressure_data.pressure = 2000.0f;
    }
    else
    {
        brakePressure_data.pressure = (brakePressure_data.voltage - 0.5f) * 500.0f;
    }
    calcBrakeBias_100Hz();
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S brakePressure_desc = {
    .moduleInit        = &brakePressure_init,
    .periodic100Hz_CLK = &brakePressure_periodic_100Hz,
};
