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

#define INIT_BOOT_DELAY                    (500)

#define CRASH_THRESH_MPS                   (8 * GRAVITY)
#define CRASH_THRESH_CONSECUTIVE_CYCLES    (3)

#define EXIT_CRASH_THRESH_G                (GRAVITY * 1.25f)
#define EXIT_CRASH_THRESH_DEG_FROM_GRAVITY (25.0f)

#define TASK_WAIT_DURATION_MS (15)
#define IMU_TIMEOUT_DURATION  (50)
#define CRASH_THRESH_MISSED_CYCLES (IMU_TIMEOUT_DURATION / TASK_WAIT_DURATION_MS)

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    crashSensor_state_E sensorState;
    float32_t accelMax;
    float32_t accelTripped;
    uint8_t   missedCycles;
} cs;

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
        const float32_t vehicleAccel = imu_getAccelNormPeak();
        if (vehicleStateOk(vehicleAccel) && !imu_isFaulted())
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
            const float32_t vehicleAccel = imu_getAccelNormPeak();
            const bool crashEvent = imu_getCrashEvent();
            cs.missedCycles = 0;

            if (cs.sensorState == CRASHSENSOR_OK)
            {
                if (crashEvent)
                {
                    crashState_data.crashLatched = true;
                    lib_nvm_requestWrite(NVM_ENTRYID_CRASH_STATE);
                    cs.accelTripped = vehicleAccel;
                    cs.accelMax = vehicleAccel;
                    cs.sensorState = CRASHSENSOR_CRASHED;
                }
                else if (imu_isFaulted())
                {
                    cs.sensorState = CRASHSENSOR_ERROR;
                }
            }
            else
            {
                const bool driverResetting = drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DRIVER_CRASH_RESET) == DRV_IO_ACTIVE;
                const bool vehicleOk = vehicleStateOk(vehicleAccel);

                cs.accelMax = vehicleAccel > cs.accelMax ? vehicleAccel : cs.accelMax;

                if (driverResetting && vehicleOk && !imu_isFaulted())
                {
                    cs.sensorState = CRASHSENSOR_OK;
                    crashState_data.crashLatched = false;
                    lib_nvm_requestWrite(NVM_ENTRYID_CRASH_STATE);
                    cs.accelTripped = 0.0f;
                    cs.accelMax = 0.0f;
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
                cs.sensorState = CRASHSENSOR_ERROR;
            }
        }

#if FEATURE_IS_ENABLED(FEATURE_CRASHSENSOR_CONTROL)
        const drv_io_activeState_E safetyState = cs.sensorState == CRASHSENSOR_OK ? DRV_IO_ACTIVE : DRV_IO_INACTIVE;
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_VCU_SFTY_EN, safetyState);
#endif
    }
}

void crashSensor_notifyFromImu(void)
{
    if (crashSensorTaskHandle != NULL)
    {
        (void)xTaskNotifyGive(crashSensorTaskHandle);
    }
}
