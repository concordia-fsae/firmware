/**
 * @file brakePressure.c
 * @brief Module source for brake pressure sensor
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "app_faultManager.h"
#include "brakePressure.h"
#include "drv_inputAD_componentSpecific.h"
#include "Module.h"
#include "ModuleDesc.h"

#include "drv_inputAD.h"
#include "Yamcan.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define BRAKE_PRESSURE_VOLTAGE_LOW_THRESHOLD     0.15f
#define BRAKE_PRESSURE_VOLTAGE_HIGH_THRESHOLD    2.85f

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

float32_t brakePressure_getBrakePressure(void)
{
    return brakePressure_data.pressure;
}

float32_t brakePressure_getBrakePressureVoltage(void)
{
    return brakePressure_data.voltage;
}

static void brakePressure_init(void)
{
    memset(&brakePressure_data, 0x00U, sizeof(brakePressure_data));
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

    app_faultManager_setFaultState(FM_FAULT_VCFRONT_BRAKEPRESSURESENSORFAULT,
                                   (brakePressure_data.voltage < BRAKE_PRESSURE_VOLTAGE_LOW_THRESHOLD) ||
                                   (brakePressure_data.voltage > BRAKE_PRESSURE_VOLTAGE_HIGH_THRESHOLD));
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S brakePressure_desc = {
    .moduleInit        = &brakePressure_init,
    .periodic100Hz_CLK = &brakePressure_periodic_100Hz,
};
