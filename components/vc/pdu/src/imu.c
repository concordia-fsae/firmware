/**
 * @file imu.c
 * @brief  Source code for imu Application
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "imu.h"
#include "drv_asm330.h"
#include "Module.h"
#include "FreeRTOS.h"
#include "task.h"
#include "string.h"
#include "app_faultManager.h"
#include "lib_buffer.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define IMU_TIMEOUT_MS 1000U

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

drv_asm330_S asm330 = {
    .dev = HW_SPI_DEV_IMU,
    .config = {
        .odr = ASM330LHB_XL_ODR_1667Hz,
        .scaleA = ASM330LHB_8g,
    },
};

struct imu_S {
    drv_imu_accel_S accel;
    drv_imu_gyro_S  gyro;
    drv_timer_S     imuTimeout;

    bool sleepCycle;
} imu;

static LIB_BUFFER_FIFO_CREATE(imuBuffer, drv_asm330_fifoElement_S, 100U) = { 0 };

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void imu_getAccel(drv_imu_accel_S* accel)
{
    taskENTER_CRITICAL();
    memcpy(accel, &imu.accel, sizeof(*accel));
    taskEXIT_CRITICAL();
}

drv_imu_accel_S* imu_getAccelRef(void)
{
    return &imu.accel;
}

void imu_getGyro(drv_imu_gyro_S* gyro)
{
    taskENTER_CRITICAL();
    memcpy(gyro, &imu.gyro, sizeof(*gyro));
    taskEXIT_CRITICAL();
}

drv_imu_gyro_S* imu_getGyroRef(void)
{
    return &imu.gyro;
}

/**
 * @brief  imu Module Init function
 */
static void imu_init()
{
    drv_asm330_init(&asm330);
    drv_timer_init(&imu.imuTimeout);
}

/**
 * @brief  imu Module 1Hz periodic function
 */
static void imu1kHz_PRD(void)
{
    if (drv_asm330_getState(&asm330) == DRV_ASM330_STATE_RUNNING)
    {
        const bool sleepCycle = imu.sleepCycle;
        imu.sleepCycle = !imu.sleepCycle;
        if (sleepCycle)
        {
            return;
        }

        const bool wasOverrun = drv_asm330_getFifoOverrun(&asm330);

        while (LIB_BUFFER_FIFO_GETLENGTH(&imuBuffer))
        {
            drv_asm330_fifoElement_S* e = &LIB_BUFFER_FIFO_POP(&imuBuffer);
            drv_imu_vector_S tmp = {0};
            asm330lhb_fifo_tag_t tag = drv_asm330_unpackElement(&asm330, e, &tmp);
            uint8_t* data = 0U;

            switch (tag)
            {
                case ASM330LHB_GYRO_NC_TAG:
                    data = (uint8_t*)&imu.gyro;
                    break;
                case ASM330LHB_XL_NC_TAG:
                    data = (uint8_t*)&imu.accel;
                    break;
                case ASM330LHB_TEMPERATURE_TAG:
                case ASM330LHB_TIMESTAMP_TAG:
                case ASM330LHB_CFG_CHANGE_TAG:
                    break;
            }

            taskENTER_CRITICAL();
            if (data) memcpy(data, (uint8_t*)&tmp, sizeof(tmp));
            taskEXIT_CRITICAL();
        }

        size_t maxContinuous = LIB_BUFFER_FIFO_GETMAXCONTINUOUS(&imuBuffer);
        drv_asm330_fifoElement_S* reserveStart = &LIB_BUFFER_FIFO_PEEKEND(&imuBuffer);
        uint16_t elements = drv_asm330_getFifoElementsReady(&asm330);
        elements = elements > maxContinuous ? (uint16_t)maxContinuous : elements;
        LIB_BUFFER_FIFO_RESERVE(&imuBuffer, elements);
        const bool imuRan = drv_asm330_getFifoElementsDMA(&asm330, (uint8_t*)reserveStart, (uint16_t)(elements * sizeof(*reserveStart)));

        if (imuRan)
        {
            drv_timer_start(&imu.imuTimeout, IMU_TIMEOUT_MS);
        }

        app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUOVERRUN, wasOverrun);
        app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUERROR, drv_timer_getState(&imu.imuTimeout) == DRV_TIMER_EXPIRED);
    }
    else
    {
        app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUOVERRUN, false);
        app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUERROR, true);
    }
}

/**
 * @brief  imu Module descriptor
 */
const ModuleDesc_S imu_desc = {
    .moduleInit       = &imu_init,
    .periodic1kHz_CLK = &imu1kHz_PRD,
};
