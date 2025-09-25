/**
 * @file brakePressure.c
 * @brief Module source for brake pressure sensor
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "brakePressure.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "drv_inputAD_componentSpecific.h"
#include "MessageUnpack_generated.h"
#include "drv_inputAD.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    float32_t voltage;
    float32_t pressure;
} brakePressure_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t brakePressure_getBrakePressure(void){
    return brakePressure_data.pressure;
}

static void brakePressure_init(void)
{
    memset(&brakePressure_data, 0x00U, sizeof(brakePressure_data));
}

static void brakePressure_periodic_100Hz(void){

    brakePressure_data.voltage = drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_BR_PR); 

    if (brakePressure_data.voltage <= 0.5f)
    {
        brakePressure_data.pressure = 0.0f;
    }
    else if(brakePressure_data.voltage >= 4.5f)
    {
        brakePressure_data.pressure = 2000.0f;
    }
    else{
        brakePressure_data.pressure = (brakePressure_data.voltage-0.5f) * 500.0f;
    }

}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S brakePressure_desc = {
    .moduleInit = &brakePressure_init,
    .periodic100Hz_CLK = &brakePressure_periodic_100Hz,
};
