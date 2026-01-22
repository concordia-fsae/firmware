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
#define BASELINE_SAMPLES 500U

/*
 * Produces a 3x3 rotation matrix R such that:
 *   R * normalize(meas) = (0,0,1)
 */
#define CALC_ROTMAX_TO_Z3(meas, R) \
    _Static_assert(COLS(meas) == 3U, "meas must be a 3D col vector"); \
    _Static_assert(LIB_LINALG_CHECK_SIZE_RMAT(R, 3U), "R must be 3x3"); \
    do { \
        float32_t n = 0.0f; \
        LIB_LINALG_GETNORM_CVEC((meas), &n); \
        LIB_LINALG_SETIDENTITY_RMAT(R); \
        \
        if (n <= (EPS)) { \
            break; \
        } \
        \
        const float32_t invn = 1.0f / n; \
        const float32_t ux = (meas)->elemCol[0] * invn; \
        const float32_t uy = (meas)->elemCol[1] * invn; \
        const float32_t uz = (meas)->elemCol[2] * invn; \
        \
        const float32_t c = uz; \
        const float32_t vx = uy; \
        const float32_t vy = -ux; \
        const float32_t s2 = vx * vx + vy * vy; \
        if (s2 < EPS * EPS) \
        { \
            if (c > 0.0f) { \
                break; \
            } else { \
                (R)->rows[0][0]=1;  (R)->rows[0][1]=0;  (R)->rows[0][2]=0; \
                (R)->rows[1][0]=0;  (R)->rows[1][1]=-1; (R)->rows[1][2]=0; \
                (R)->rows[2][0]=0;  (R)->rows[2][1]=0;  (R)->rows[2][2]=-1; \
                break; \
            } \
        } \
        \
        const float32_t s = sqrtf(s2); \
        const float32_t invs = 1.0f / s; \
        const float32_t kx = vx * invs; \
        const float32_t ky = vy * invs; \
        \
        const float32_t one_mc = 1.0f - c; \
        \
        (R)->rows[0][0] = 1.0f - one_mc * ky * ky; \
        (R)->rows[0][1] = one_mc * kx * ky; \
        (R)->rows[0][2] = s * ky; \
        (R)->rows[1][0] = one_mc * kx * ky; \
        (R)->rows[1][1] = 1.0f - one_mc * kx * kx; \
        (R)->rows[1][2] = -s * kx; \
        (R)->rows[2][0] = -s * ky; \
        (R)->rows[2][1] = s * kx; \
        (R)->rows[2][2] = c; \
    } while (0)

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    INIT = 0x00U,
    STABILIZING,
    BASELINING,
    ZEROING,
    RUNNING,
} operatingMode_E;

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
    operatingMode_E operatingMode;
} imu;

static LIB_BUFFER_FIFO_CREATE(imuBuffer, drv_asm330_fifoElement_S, 100U) = { 0 };

const drv_imu_vectorTransform_S rotationToVehicleFrame = {
    .rows = {
        { 0, 1, 0 },
        { 1, 0, 0 },
        { 0, 0, 1 },
    },
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static uint16_t averageSamples(drv_imu_vector_S* vec)
{
    uint16_t count = 0;

    // Wait until the next element is being filled so we know our current element is valid
    while (LIB_BUFFER_FIFO_PEEKN(&imuBuffer, 1).tag)
    {
        drv_imu_vector_S tmp = {0};
        drv_asm330_fifoElement_S* e = &LIB_BUFFER_FIFO_POP(&imuBuffer);
        asm330lhb_fifo_tag_t tag = drv_asm330_unpackElement(&asm330, e, &tmp);
        e->tag = 0U; // Clear the tag so its empty when we get back to this FIFO element

        switch (tag)
        {
            case ASM330LHB_XL_NC_TAG:
                LIB_LINALG_SUM_CVEC(vec, &tmp, vec);
                count++;
                break;
            default:
                break;
        }
    }

    if (count)
    {
        LIB_LINALG_MUL_CVECSCALAR(vec, (1.0f / count), vec);
    }

    return count;
}

static bool calculateTransform(void)
{
    static drv_imu_vector_S sum = {0};
    static uint16_t count = 0;
    drv_imu_vector_S tmp = {0};
    drv_imu_vectorTransform_S tmpTransform = {0};

    count += averageSamples(&tmp);
    LIB_LINALG_SUM_CVEC(&sum, &tmp, &sum);
    LIB_LINALG_MUL_CVECSCALAR(&sum, (1.0f / 2.0f), &sum);

    const bool valid = count > BASELINE_SAMPLES;

    if (valid)
    {
        CALC_ROTMAX_TO_Z3(&sum, &tmpTransform);
        LIB_LINALG_MUL_RMATRMAT_SET(&tmpTransform, &rotationToVehicleFrame, &imuCalibration_data.rotation);
        LIB_LINALG_CLEAR_CVEC(&sum);
        count = 0;
    }

    return valid;
}

static bool calculateOffset(void)
{
    static drv_imu_vector_S sum = {0};
    static uint16_t count = 0;
    drv_imu_vector_S tmp = {0};

    count += averageSamples(&tmp);
    LIB_LINALG_SUM_CVEC(&sum, &tmp, &sum);
    LIB_LINALG_MUL_CVECSCALAR(&sum, (1.0f / 2.0f), &sum);

    const bool valid = count > BASELINE_SAMPLES;

    if (valid)
    {
        LIB_LINALG_MUL_RMATCVEC_SET(&imuCalibration_data.rotation, &sum, &imuCalibration_data.zeroAccel);
        LIB_LINALG_MUL_CVECSCALAR(&imuCalibration_data.zeroAccel, -1.0f, &imuCalibration_data.zeroAccel);
        ((drv_imu_accel_S*)&imuCalibration_data.zeroAccel)->accelZ += GRAVITY;
        LIB_LINALG_CLEAR_CVEC(&sum);
        count = 0;
    }

    return valid;
}

static bool egressFifo(void)
{
    uint16_t elements = drv_asm330_getFifoElementsReady(&asm330);

    size_t maxContinuous = LIB_BUFFER_FIFO_GETMAXCONTINUOUS(&imuBuffer);
    drv_asm330_fifoElement_S* reserveStart = &LIB_BUFFER_FIFO_PEEKEND(&imuBuffer);
    elements = elements > maxContinuous ? (uint16_t)maxContinuous : elements;
    LIB_BUFFER_FIFO_RESERVE(&imuBuffer, elements);
    return drv_asm330_getFifoElementsDMA(&asm330, (uint8_t*)reserveStart, (uint16_t)(elements * sizeof(*reserveStart)));
}

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

    imu.operatingMode = RUNNING;
}

/**
 * @brief  imu Module 100Hz periodic function
 */
static void imu100Hz_PRD(void)
{
    switch (imu.operatingMode)
    {
        case INIT:
            if (egressFifo())
            {
                imu.operatingMode = STABILIZING;
            }
            break;
        case STABILIZING:
            if (calculateTransform())
            {
                imu.operatingMode = BASELINING;
            }
            egressFifo();
            break;
        case BASELINING:
            if (calculateTransform())
            {
                imu.operatingMode = ZEROING;
            }
            egressFifo();
            break;
        case ZEROING:
            if (calculateOffset())
            {
                imu.operatingMode = RUNNING;
            }
            egressFifo();
            break;
        case RUNNING:
            if ((drv_asm330_getState(&asm330) == DRV_ASM330_STATE_RUNNING))
            {
                const bool wasOverrun = drv_asm330_getFifoOverrun(&asm330);
                const bool imuRan = egressFifo();

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
            break;
    }
}

/**
 * @brief  imu Module 1kHz periodic function
 */
static void imu1kHz_PRD(void)
{
    if ((imu.operatingMode == RUNNING) && (drv_asm330_getState(&asm330) == DRV_ASM330_STATE_RUNNING))
    {
        // Wait until the next element is being filled so we know our current element is valid
        while (LIB_BUFFER_FIFO_PEEKN(&imuBuffer, 1).tag)
        {
            drv_asm330_fifoElement_S* e = &LIB_BUFFER_FIFO_POP(&imuBuffer);
            drv_imu_vector_S tmp = {0};
            drv_imu_vector_S rotated = {0};
            asm330lhb_fifo_tag_t tag = drv_asm330_unpackElement(&asm330, e, &tmp);
            e->tag = 0U; // Clear the tag so its empty when we get back to this FIFO element
            uint8_t* data = 0U;

            switch (tag)
            {
               case ASM330LHB_GYRO_NC_TAG:
                    data = (uint8_t*)&imu.gyro;
                    LIB_LINALG_MUL_RMATCVEC_SET(&imuCalibration_data.rotation, &tmp, &rotated);
                    break;
                case ASM330LHB_XL_NC_TAG:
                    data = (uint8_t*)&imu.accel;
                    LIB_LINALG_MUL_RMATCVEC_SET(&imuCalibration_data.rotation, &tmp, &rotated);
                    LIB_LINALG_SUM_CVEC(&rotated, &imuCalibration_data.zeroAccel, &rotated);
                    break;
                case ASM330LHB_TEMPERATURE_TAG:
                case ASM330LHB_TIMESTAMP_TAG:
                case ASM330LHB_CFG_CHANGE_TAG:
                    break;
            }

            taskENTER_CRITICAL();
            if (data) memcpy(data, (uint8_t*)&rotated, sizeof(rotated));
            taskEXIT_CRITICAL();
        }
    }
}

/**
 * @brief  imu Module descriptor
 */
const ModuleDesc_S imu_desc = {
    .moduleInit        = &imu_init,
    .periodic100Hz_CLK = &imu100Hz_PRD,
    .periodic1kHz_CLK  = &imu1kHz_PRD,
};
