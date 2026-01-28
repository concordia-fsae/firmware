/**
 * @file crashSensor.c
 * @brief  Crash sensor task source for VCPDU
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "crashSensor.h"
#include "FreeRTOS.h"
#include "imu.h"
#include "math.h"
#include "powerManager.h"
#include "stdbool.h"
#include "task.h"
#include "drv_inputAD.h"
#include "drv_outputAD.h"
#include "MessageUnpack_generated.h"
#include <string.h>
#include "app_vehicleState.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(FEATURE_CRASHSENSOR)
#define INIT_BOOT_DELAY                     (500)

#define CRASH_THRESH_MPS                   (8 * GRAVITY)
#define CRASH_THRESH_CONSECUTIVE_CYCLES    (3)

#define EXIT_CRASH_THRESH_G                (GRAVITY * 1.1f)
#define EXIT_CRASH_THRESH_DEG_FROM_GRAVITY (25.0f)

#define TASK_WAIT_DURATION_MS (15)
#define IMU_TIMEOUT_DURATION  (50)
#define CRASH_THRESH_MISSED_CYCLES (IMU_TIMEOUT_DURATION / TASK_WAIT_DURATION_MS)
#endif

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    crashSensor_state_E sensorState;
    float32_t accelMax;
    float32_t accelTripped;
#if FEATURE_IS_ENABLED(FEATURE_CRASHSENSOR)
    uint8_t   missedCycles;
    uint8_t   egregiousCycles;
#endif
} cs;

#if FEATURE_IS_ENABLED(FEATURE_CRASHSENSOR)
static TaskHandle_t crashSensorTaskHandle = NULL;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static bool vehicleStateOk(const float32_t currentAccel)
{
    const float32_t angleFromGravity = imu_getAngleFromGravity();
    bool ret = false;

    if ((currentAccel < EXIT_CRASH_THRESH_G) &&
        (angleFromGravity < EXIT_CRASH_THRESH_DEG_FROM_GRAVITY))
    {
        ret = true;
    }

    return ret;
}
#endif

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t crashSensor_getTrippedAcceleration(void)
{
    return cs.accelTripped;
}

float32_t crashSensor_getMaxAcceleration(void)
{
    return cs.accelMax;
}

crashSensor_state_E crashSensor_getState(void)
{
    return cs.sensorState;
}

CAN_crashSensorState_E crashSensor_getStateCAN(void)
{
    switch (cs.sensorState)
    {
        case CRASHSENSOR_OK:
            return CAN_CRASHSENSORSTATE_OK;
        case CRASHSENSOR_CRASHED:
            return CAN_CRASHSENSORSTATE_CRASHED;
        case CRASHSENSOR_ERROR:
            return CAN_CRASHSENSORSTATE_ERROR;
        case CRASHSENSOR_INIT:
        default:
            return CAN_CRASHSENSORSTATE_SNA;
    }
}

void crashSensor_task(void)
{
    memset(&cs, 0x00U, sizeof(cs));

#if FEATURE_IS_ENABLED(FEATURE_CRASHSENSOR)
    crashSensorTaskHandle = xTaskGetCurrentTaskHandle();

    if (crashState_data.crashLatched)
    {
        cs.sensorState = CRASHSENSOR_CRASHED;
    }
    else
    {
        cs.sensorState = CRASHSENSOR_INIT;
    }

    vTaskDelay(pdMS_TO_TICKS(INIT_BOOT_DELAY));

    if (cs.sensorState == CRASHSENSOR_INIT)
    {
        const float32_t vehicleAccel = imu_getAccelNorm();
        if (vehicleStateOk(vehicleAccel))
        {
            cs.sensorState = CRASHSENSOR_OK;
            drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_VCU_SFTY_EN, DRV_IO_ACTIVE);
        }
        else
        {
            cs.sensorState = CRASHSENSOR_ERROR;
        }
    }

    for (;;)
    {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TASK_WAIT_DURATION_MS)))
        {
            const float32_t vehicleAccel = imu_getAccelNorm();
            cs.missedCycles = 0;

            if (cs.sensorState == CRASHSENSOR_OK)
            {
                if (vehicleAccel >= CRASH_THRESH_MPS)
                {
                    if (cs.egregiousCycles < CRASH_THRESH_CONSECUTIVE_CYCLES)
                    {
                        cs.egregiousCycles++;
                    }
                    else
                    {
                        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_VCU_SFTY_EN, DRV_IO_INACTIVE);
                        crashState_data.crashLatched = true;
                        lib_nvm_requestWrite(NVM_ENTRYID_CRASH_STATE);
                        cs.accelTripped = vehicleAccel;
                        cs.accelMax = vehicleAccel;
                        cs.sensorState = CRASHSENSOR_CRASHED;
                    }
                }
                else
                {
                    cs.egregiousCycles = 0;
                }
            }
            else
            {
                const bool driverResetting = drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DRIVER_CRASH_RESET) == DRV_IO_ACTIVE;
                const bool vehicleOk = vehicleStateOk(vehicleAccel);

                cs.accelMax = vehicleAccel > cs.accelMax ? vehicleAccel : cs.accelMax;

                if (driverResetting && vehicleOk)
                {
                    cs.sensorState = CRASHSENSOR_OK;
                    crashState_data.crashLatched = false;
                    lib_nvm_requestWrite(NVM_ENTRYID_CRASH_STATE);
                    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_VCU_SFTY_EN, DRV_IO_ACTIVE);
                }
            }
        }
        else
        {
            if (cs.missedCycles < CRASH_THRESH_MISSED_CYCLES)
            {
                cs.missedCycles++;
            }
            else
            {
                drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_VCU_SFTY_EN, DRV_IO_INACTIVE);
                cs.sensorState = CRASHSENSOR_ERROR;
            }
        }
    }
#else
    cs.sensorState = CRASHSENSOR_OK;
    vTaskSuspend(NULL);
#endif
}

#if FEATURE_IS_ENABLED(FEATURE_CRASHSENSOR)
void crashSensor_notifyFromImu(void)
{
    if (crashSensorTaskHandle != NULL)
    {
        (void)xTaskNotifyGive(crashSensorTaskHandle);
    }
}
#endif
