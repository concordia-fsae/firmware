/**
 * @file imu.c
 * @brief  Source code for imu Application
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "imu.h"
#include "crashSensor.h"
#include "drv_asm330.h"
#include "Module.h"
#include "FreeRTOS.h"
#include "task.h"
#include "string.h"
#include "app_faultManager.h"
#include "lib_buffer.h"
#include "lib_madgwick.h"
#include "lib_simpleFilter.h"
#include "app_vehicleState.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define IMU_IMPACT_THRESH_MPS (GRAVITY * 4)

#define IMU_WAKE_ACCEL_DELTA_MPS2 0.4f
#define IMU_WAKE_GYRO_DPS 3.0f
#define IMU_WAKE_HITS_REQUIRED 5U
#define IMU_WAKE_DELAY_MS (15U * 60000U)

#define IMU_TIMEOUT_MS 1000U
#define BASELINE_SAMPLES 500U
#define MADGWICK_BETA 0.1f

#define RAD_TO_DEG (180.0f / 3.14159265358979323846f)
#define DEG_TO_RAD (1 / RAD_TO_DEG)

#define IMU_LPF_CUTOFF_HZ 100.0f
#define IMU_LPF_DT_S      0.01f

#define IMU_FSM_CRASH_PROGRAM_NUMBER  1U
#define IMU_FSM_IMPACT_PROGRAM_NUMBER 2U
#define IMU_FSM_CRASH_PROGRAM_SIZE    12U
#define IMU_FSM_IMPACT_PROGRAM_SIZE   12U
#define IMU_FSM_ARM_CYCLES            10U

#define IMU_FSM_BASE_START_ADDR       0x0400U
#define IMU_FSM_CRASH_START_ADDR      IMU_FSM_BASE_START_ADDR
#define IMU_FSM_IMPACT_START_ADDR     (IMU_FSM_CRASH_START_ADDR + IMU_FSM_CRASH_PROGRAM_SIZE)

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
    INIT_VEHICLEANGLE,
    STABILIZING,
    STABILIZING_VEHICLEANGLE,
    BASELINING,
    ZEROING,
    GET_VEHICLEANGLE,
    RUNNING,
} operatingMode_E;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

drv_asm330_S asm330 = {
    .dev = HW_SPI_DEV_IMU,
    .config = {
        .odr = ASM330LHB_XL_ODR_1667Hz,
        .scaleA = ASM330LHB_16g,
    },
};

struct imu_S {
    drv_imu_accel_S accel;
    drv_imu_gyro_S  gyro;
    drv_imu_gyro_S  vehicleAngle;
    drv_timer_S     imuTimeout;
    operatingMode_E operatingMode;
    uint64_t        lastCycle_us;
    lib_madgwick_S  madgwick;
    float32_t       accelNorm;
    float32_t       accelNormPeak;
    float32_t       angleFromGravity;
    bool            fsmCrashInitOk;
    bool            fsmImpactInitOk;
    bool            fsmCrashEvent;
    bool            fsmImpactActive;
    float32_t       impactAccelMax;
    uint8_t         fsmArmCyclesLeft;
    bool            calibrating;
} imu;

static LIB_BUFFER_FIFO_CREATE(imuBuffer, drv_asm330_fifoElement_S, 100U) = { 0 };
static struct
{
    drv_imu_accel_S prevAccel;
    uint8_t         hitCount;
    bool            hasPrev;
} imuWake = { 0 };

static struct
{
    lib_simpleFilter_lpf_S accelX;
    lib_simpleFilter_lpf_S accelY;
    lib_simpleFilter_lpf_S accelZ;
    lib_simpleFilter_lpf_S gyroX;
    lib_simpleFilter_lpf_S gyroY;
    lib_simpleFilter_lpf_S gyroZ;
    bool accelInit;
    bool gyroInit;
} imuLpf = { 0 };

static const uint8_t imuFsmCrashProgram[IMU_FSM_CRASH_PROGRAM_SIZE] = {
    0x50U, /* CONFIG_A: 1 threshold, 1 mask */
    0x00U, /* CONFIG_B */
    0x0CU, /* SIZE */
    0x00U, /* SETTINGS */
    0x00U, /* RESET_POINTER */
    0x00U, /* PROGRAM_POINTER */
    0x00U, /* THRESH1 (LSB) = 8.0g */
    0x48U, /* THRESH1 (MSB) */
    0x03U, /* MASKA: +/-V */
    0x00U, /* TMASKA */
    0x05U, /* NOP | GNTH1 */
    0x22U, /* CONTREL */
};

static const uint8_t imuFsmImpactProgram[IMU_FSM_IMPACT_PROGRAM_SIZE] = {
    0x50U, /* CONFIG_A: 1 threshold, 1 mask */
    0x00U, /* CONFIG_B */
    0x0CU, /* SIZE */
    0x00U, /* SETTINGS */
    0x00U, /* RESET_POINTER */
    0x00U, /* PROGRAM_POINTER */
    0x00U, /* THRESH1 (LSB) = 4.0g */
    0x44U, /* THRESH1 (MSB) */
    0x03U, /* MASKA: +/-V */
    0x00U, /* TMASKA */
    0x05U, /* NOP | GNTH1 */
    0x22U, /* CONTREL */
};

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

static void averageSamples(drv_imu_vector_S* vecA, uint16_t* countA, drv_imu_vector_S* vecG, uint16_t* countG)
{
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
                LIB_LINALG_SUM_CVEC(vecA, &tmp, vecA);
                (*countA)++;
                break;
            case ASM330LHB_GYRO_NC_TAG:
                if (vecG && countG)
                {
                    LIB_LINALG_SUM_CVEC(vecG, &tmp, vecG);
                    (*countG)++;
                }
                break;
            default:
                break;
        }
    }
}

static bool disregardSamples(void)
{
    static uint16_t countA = 0;
    static uint16_t countG = 0;
    drv_imu_vector_S tmp = {0};

    averageSamples(&tmp, &countA, &tmp, &countG);

    if (countA + countG > BASELINE_SAMPLES)
    {
        countA = 0;
        countG = 0;

        return true;
    }
    else
    {
        return false;
    }
}

static bool calculateTransform(void)
{
    static drv_imu_vector_S sum = {0};
    static uint16_t count = 0;
    drv_imu_vector_S tmp = {0};
    drv_imu_vectorTransform_S tmpTransform = {0};

    averageSamples(&tmp, &count, NULL, NULL);
    LIB_LINALG_SUM_CVEC(&sum, &tmp, &sum);

    const bool valid = count > BASELINE_SAMPLES;

    if (valid)
    {
        LIB_LINALG_MUL_CVECSCALAR(&sum, (1.0f / count), &sum);
        CALC_ROTMAX_TO_Z3(&sum, &tmpTransform);
        LIB_LINALG_MUL_RMATRMAT_SET(&tmpTransform, &rotationToVehicleFrame, &imuCalibration_data.rotation);
        LIB_LINALG_CLEAR_CVEC(&sum);
        count = 0;
    }

    return valid;
}

static bool calculateOffset(void)
{
    static drv_imu_vector_S sumA = {0};
    static uint16_t countA = 0;
    static drv_imu_vector_S sumG = {0};
    static uint16_t countG = 0;
    drv_imu_vector_S tmpA = {0};
    drv_imu_vector_S tmpG = {0};

    averageSamples(&tmpA, &countA, &tmpG, &countG);
    LIB_LINALG_SUM_CVEC(&sumA, &tmpA, &sumA);
    LIB_LINALG_SUM_CVEC(&sumG, &tmpG, &sumG);

    const bool validA = countA > BASELINE_SAMPLES;
    const bool validG = countG > BASELINE_SAMPLES;
    const bool valid = validA && validG;

    if (valid)
    {
        LIB_LINALG_MUL_CVECSCALAR(&sumA, (1.0f / countA), &sumA);
        LIB_LINALG_MUL_RMATCVEC_SET(&imuCalibration_data.rotation, &sumA, &imuCalibration_data.zeroAccel);
        LIB_LINALG_MUL_CVECSCALAR(&imuCalibration_data.zeroAccel, -1.0f, &imuCalibration_data.zeroAccel);
        ((drv_imu_accel_S*)&imuCalibration_data.zeroAccel)->accelZ += GRAVITY;

        LIB_LINALG_MUL_CVECSCALAR(&sumG, (1.0f / countG), &sumG);
        LIB_LINALG_MUL_RMATCVEC_SET(&imuCalibration_data.rotation, &sumG, &imuCalibration_data.zeroGyro);
        LIB_LINALG_MUL_CVECSCALAR(&imuCalibration_data.zeroGyro, -1.0f, &imuCalibration_data.zeroGyro);

        LIB_LINALG_CLEAR_CVEC(&sumA);
        LIB_LINALG_CLEAR_CVEC(&sumG);
        countA = 0;
        countG = 0;
    }

    return valid;
}

static void updateMadgwick(lib_madgwick_S* f, lib_madgwick_euler_S* g, lib_madgwick_euler_S* a)
{
    const uint64_t currentTime = HW_TIM_getBaseTick();
    const float32_t dt = (float32_t)(currentTime - imu.lastCycle_us) / 1000000.0f;

    madgwick_update_imu(f, g, a, dt);
    imu.lastCycle_us = currentTime;
}

static void correctThenSetVector(drv_imu_vector_S* in, drv_imu_vector_S* zero, drv_imu_vector_S* out);
static void lpfAccel(drv_imu_accel_S* accel);
static void lpfGyro(drv_imu_gyro_S* gyro);
static bool calculateVehicleAngle(void)
{
    static uint16_t countA = 0;
    static uint16_t countG = 0;
    uint16_t tmpCountA = 0;
    uint16_t tmpCountG = 0;

    drv_imu_vector_S tmpA = {0};
    drv_imu_vector_S tmpG = {0};
    drv_imu_vector_S gVec = {0};
    drv_imu_vector_S aVec = {0};

    averageSamples(&tmpA, &tmpCountA, &tmpG, &tmpCountG);

    if (countA)
    {
        LIB_LINALG_MUL_CVECSCALAR(&tmpA, (1.0f / tmpCountA), &tmpA);
        correctThenSetVector(&tmpA, &imuCalibration_data.zeroAccel, (drv_imu_vector_S*)&imu.accel);
        LIB_LINALG_MUL_RMATCVEC_SET(&imuCalibration_data.rotation, &tmpA, &aVec);
    }
    else
    {
        LIB_LINALG_CVEC_EQ_CVEC((drv_imu_vector_S*)&imu.accel, &tmpA);
    }
    if (countG)
    {
        LIB_LINALG_MUL_CVECSCALAR(&tmpG, (1.0f / tmpCountG), &tmpG);
        correctThenSetVector(&tmpG, &imuCalibration_data.zeroGyro, (drv_imu_vector_S*)&imu.gyro);
        LIB_LINALG_MUL_RMATCVEC_SET(&imuCalibration_data.rotation, &tmpG, &gVec);
    }
    else
    {
        LIB_LINALG_CVEC_EQ_CVEC((drv_imu_vector_S*)&imu.gyro, &tmpG);
    }

    updateMadgwick(&imu.madgwick, (drv_imu_euler_S*)&gVec, (drv_imu_euler_S*)&aVec);

    countA += tmpCountA;
    countG += tmpCountG;

    const bool valid = (countG > BASELINE_SAMPLES);
    if (!valid)
    {
        return false;
    }

    countA = 0;
    countG = 0;

    return true;
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

static void correctThenSetVector(drv_imu_vector_S* in, drv_imu_vector_S* zero, drv_imu_vector_S* out)
{
    drv_imu_vector_S rotated = {0};
    LIB_LINALG_MUL_RMATCVEC_SET(&imuCalibration_data.rotation, in, &rotated);
    LIB_LINALG_SUM_CVEC(&rotated, zero, &rotated);
    taskENTER_CRITICAL();
    LIB_LINALG_CVEC_EQ_CVEC(&rotated, out);
    taskEXIT_CRITICAL();
}

static void lpfAccel(drv_imu_accel_S* accel)
{
    if (imuLpf.accelInit)
    {
        imuLpf.accelInit = false;
        imuLpf.accelX.y = accel->accelX;
        imuLpf.accelY.y = accel->accelY;
        imuLpf.accelZ.y = accel->accelZ;
        lib_madgwick_euler_S initAccel = {
            .x = accel->accelX,
            .y = accel->accelY,
            .z = accel->accelZ,
        };
        madgwick_init_quaternion_from_accel(&imu.madgwick, &initAccel);
    }

    accel->accelX = lib_simpleFilter_lpf_step(&imuLpf.accelX, accel->accelX);
    accel->accelY = lib_simpleFilter_lpf_step(&imuLpf.accelY, accel->accelY);
    accel->accelZ = lib_simpleFilter_lpf_step(&imuLpf.accelZ, accel->accelZ);
}

static void lpfGyro(drv_imu_gyro_S* gyro)
{
    if (imuLpf.gyroInit)
    {
        imuLpf.gyroInit = false;
        imuLpf.gyroX.y = gyro->rotX;
        imuLpf.gyroY.y = gyro->rotY;
        imuLpf.gyroZ.y = gyro->rotZ;
    }

    gyro->rotX = lib_simpleFilter_lpf_step(&imuLpf.gyroX, gyro->rotX);
    gyro->rotY = lib_simpleFilter_lpf_step(&imuLpf.gyroY, gyro->rotY);
    gyro->rotZ = lib_simpleFilter_lpf_step(&imuLpf.gyroZ, gyro->rotZ);
}

static void resetWakeDetector(void)
{
    imuWake.hitCount = 0U;
    imuWake.hasPrev = false;
}

static bool detectSleepWakeMovement(void)
{
    if (!imuWake.hasPrev)
    {
        imuWake.prevAccel = imu.accel;
        imuWake.hasPrev = true;
        return false;
    }

    const float32_t dAx = imu.accel.accelX - imuWake.prevAccel.accelX;
    const float32_t dAy = imu.accel.accelY - imuWake.prevAccel.accelY;
    const float32_t dAz = imu.accel.accelZ - imuWake.prevAccel.accelZ;
    const float32_t accelDelta = sqrtf((dAx * dAx) + (dAy * dAy) + (dAz * dAz));

    const float32_t gX = imu.gyro.rotX;
    const float32_t gY = imu.gyro.rotY;
    const float32_t gZ = imu.gyro.rotZ;
    const float32_t gyroMag = sqrtf((gX * gX) + (gY * gY) + (gZ * gZ));

    imuWake.prevAccel = imu.accel;
    if ((accelDelta > IMU_WAKE_ACCEL_DELTA_MPS2) || (gyroMag > IMU_WAKE_GYRO_DPS))
    {
        if (imuWake.hitCount < IMU_WAKE_HITS_REQUIRED)
        {
            imuWake.hitCount++;
        }
    }
    else
    {
        imuWake.hitCount = 0U;
    }

    if (imuWake.hitCount >= IMU_WAKE_HITS_REQUIRED)
    {
        imuWake.hitCount = 0U;
        return true;
    }

    return false;
}

static void transitionImuState(void)
{
    switch (imu.operatingMode)
    {
        case INIT:
        case INIT_VEHICLEANGLE:
            if (egressFifo())
            {
                imu.operatingMode = imu.operatingMode == INIT_VEHICLEANGLE ? STABILIZING_VEHICLEANGLE : STABILIZING;
            }
            break;
        case STABILIZING:
        case STABILIZING_VEHICLEANGLE:
            if (disregardSamples())
            {
                imu.operatingMode = imu.operatingMode == STABILIZING_VEHICLEANGLE ? GET_VEHICLEANGLE : BASELINING;
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
                lib_nvm_requestWrite(NVM_ENTRYID_IMU_CALIB);
                imu.operatingMode = RUNNING;
                imu.calibrating = false;
            }
            egressFifo();
            break;
        case GET_VEHICLEANGLE:
            if (calculateVehicleAngle())
            {
                imu.operatingMode = RUNNING;
            }
            egressFifo();
            break;
        case RUNNING:
            break;
    }
}

static void handleImuSamples(void)
{
    if ((imu.operatingMode == RUNNING) && (drv_asm330_getState(&asm330) == DRV_ASM330_STATE_RUNNING))
    {
        drv_imu_vector_S sumA = {0};
        drv_imu_vector_S sumG = {0};
        uint16_t countA = 0;
        uint16_t countG = 0;

        averageSamples(&sumA, &countA, &sumG, &countG);

        if (countA)
        {
            LIB_LINALG_MUL_CVECSCALAR(&sumA, (1.0f / countA), &sumA);
            drv_imu_accel_S accel = { 0 };
            correctThenSetVector(&sumA, &imuCalibration_data.zeroAccel, (drv_imu_vector_S*)&accel);
            LIB_LINALG_GETNORM_CVEC((drv_imu_vector_S*)&accel, &imu.accelNormPeak);
            lpfAccel(&accel);
            LIB_LINALG_GETNORM_CVEC((drv_imu_vector_S*)&accel, &imu.accelNorm);
            taskENTER_CRITICAL();
            memcpy(&imu.accel, &accel, sizeof(imu.accel));
            taskEXIT_CRITICAL();
        }
        if (countG)
        {
            LIB_LINALG_MUL_CVECSCALAR(&sumG, (1.0f / countG), &sumG);
            drv_imu_gyro_S gyro = { 0 };
            correctThenSetVector(&sumG, &imuCalibration_data.zeroGyro, (drv_imu_vector_S*)&gyro);
            lpfGyro(&gyro);
            taskENTER_CRITICAL();
            memcpy(&imu.gyro, &gyro, sizeof(imu.gyro));
            taskEXIT_CRITICAL();
        }

        updateMadgwick(&imu.madgwick, (drv_imu_euler_S*)&imu.gyro, (drv_imu_euler_S*)&imu.accel);

        taskENTER_CRITICAL();
        madgwick_get_euler_deg(&imu.madgwick, (drv_imu_euler_S*)&imu.vehicleAngle);
        taskEXIT_CRITICAL();

        const float32_t rollRad = imu.vehicleAngle.rotX * DEG_TO_RAD;
        const float32_t pitchRad = imu.vehicleAngle.rotY * DEG_TO_RAD;
        float32_t cosTheta = cosf(rollRad) * cosf(pitchRad);
        if (cosTheta > 1.0f)
        {
            cosTheta = 1.0f;
        }
        else if (cosTheta < -1.0f)
        {
            cosTheta = -1.0f;
        }
        imu.angleFromGravity = acosf(cosTheta) * RAD_TO_DEG;
    }
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

void imu_getVehicleAngle(drv_imu_gyro_S* gyro)
{
    taskENTER_CRITICAL();
    memcpy(gyro, &imu.vehicleAngle, sizeof(*gyro));
    taskEXIT_CRITICAL();
}

float32_t imu_getAccelNorm(void)
{
    return imu.accelNorm;
}

float32_t imu_getAccelNormPeak(void)
{
    return imu.accelNormPeak;
}

float32_t imu_getAngleFromGravity(void)
{
    return imu.angleFromGravity;
}

drv_imu_gyro_S* imu_getVehicleAngleRef(void)
{
    return &imu.vehicleAngle;
}

bool imu_getCrashEvent(void)
{
    bool event = false;

    taskENTER_CRITICAL();
    event = imu.fsmCrashEvent;
    imu.fsmCrashEvent = false;
    taskEXIT_CRITICAL();

    return event;
}

bool imu_getImpactActive(void)
{
    return imu.fsmImpactActive;
}

float32_t imu_getImpactAccelCurrent(void)
{
    return imu.fsmImpactActive ? imu.accelNormPeak : 0.0f;
}

float32_t imu_getImpactAccelMax(void)
{
    return imu.fsmImpactActive ? imu.impactAccelMax : 0.0f;
}

bool imu_isFaulted(void)
{
    return !(imu.fsmCrashInitOk && imu.fsmImpactInitOk);
}

bool imu_isCalibrating(void)
{
    return imu.calibrating;
}

/**
 * @brief  imu Module Init function
 */
static void imu_init()
{
    drv_asm330_init(&asm330);
    drv_timer_init(&imu.imuTimeout);
    madgwick_init(&imu.madgwick, MADGWICK_BETA);
    imu.lastCycle_us = HW_TIM_getBaseTick();
    lib_simpleFilter_lpf_calcSmoothingFactor(&imuLpf.accelX, IMU_LPF_CUTOFF_HZ, IMU_LPF_DT_S);
    lib_simpleFilter_lpf_calcSmoothingFactor(&imuLpf.accelY, IMU_LPF_CUTOFF_HZ, IMU_LPF_DT_S);
    lib_simpleFilter_lpf_calcSmoothingFactor(&imuLpf.accelZ, IMU_LPF_CUTOFF_HZ, IMU_LPF_DT_S);
    lib_simpleFilter_lpf_calcSmoothingFactor(&imuLpf.gyroX, IMU_LPF_CUTOFF_HZ, IMU_LPF_DT_S);
    lib_simpleFilter_lpf_calcSmoothingFactor(&imuLpf.gyroY, IMU_LPF_CUTOFF_HZ, IMU_LPF_DT_S);
    lib_simpleFilter_lpf_calcSmoothingFactor(&imuLpf.gyroZ, IMU_LPF_CUTOFF_HZ, IMU_LPF_DT_S);
    imuLpf.accelInit = true;
    imuLpf.gyroInit = true;

    imu.operatingMode = INIT_VEHICLEANGLE;

    imu.fsmCrashInitOk = drv_asm330_loadFsmProgram(&asm330,
                                                   IMU_FSM_CRASH_PROGRAM_NUMBER,
                                                   IMU_FSM_CRASH_START_ADDR,
                                                   imuFsmCrashProgram,
                                                   IMU_FSM_CRASH_PROGRAM_SIZE,
                                                   ASM330LHB_ODR_FSM_104Hz);
    imu.fsmImpactInitOk = drv_asm330_loadFsmProgram(&asm330,
                                                    IMU_FSM_IMPACT_PROGRAM_NUMBER,
                                                    IMU_FSM_IMPACT_START_ADDR,
                                                    imuFsmImpactProgram,
                                                    IMU_FSM_IMPACT_PROGRAM_SIZE,
                                                    ASM330LHB_ODR_FSM_104Hz);
    imu.fsmCrashEvent = false;
    imu.fsmImpactActive = false;
    imu.impactAccelMax = 0.0f;
    imu.fsmArmCyclesLeft = IMU_FSM_ARM_CYCLES;
    app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUFSMINITFAILURE,
                                   !(imu.fsmCrashInitOk && imu.fsmImpactInitOk));
}

/**
 * @brief  imu Module 100Hz periodic function
 */
static void imu100Hz_PRD(void)
{
    CAN_digitalStatus_E tmp = CAN_DIGITALSTATUS_SNA;
    static bool wasSleeping = false;
    const bool imuCalibrated = !memcmp(&imuCalibration_data, &imuCalibration_default, sizeof(imuCalibration_data));
    const bool calibrate = (CANRX_get_signal(VEH, SWS_requestCalibImu, &tmp) == CANRX_MESSAGE_VALID) &&
                           (tmp == CAN_DIGITALSTATUS_ON);

    if (calibrate)
    {
        lib_nvm_clearEntry(NVM_ENTRYID_IMU_CALIB);
        imu.operatingMode = INIT;
        imu.calibrating = true;
    }

    app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUUNCALIBRATED, !imuCalibrated);

    if (imu.operatingMode == RUNNING)
    {
        CAN_digitalStatus_E tmp = CAN_DIGITALSTATUS_SNA;
        const bool calibrate = (CANRX_get_signal(VEH, SWS_requestCalibImu, &tmp) == CANRX_MESSAGE_VALID) &&
                               (tmp == CAN_DIGITALSTATUS_ON);

        if (calibrate)
        {
            lib_nvm_clearEntry(NVM_ENTRYID_IMU_CALIB);
            imu.operatingMode = INIT;
            imu.calibrating = true;
            return;
        }

        if ((drv_asm330_getState(&asm330) == DRV_ASM330_STATE_RUNNING))
        {
            const bool wasOverrun = drv_asm330_getFifoOverrun(&asm330);
            const bool imuRan = egressFifo();
            handleImuSamples();

            if (imu.fsmCrashInitOk || imu.fsmImpactInitOk)
            {
                if (imu.fsmArmCyclesLeft > 0U)
                {
                    imu.fsmArmCyclesLeft--;
                    (void)drv_asm330_clearFsmStatus(&asm330);
                }
                else
                {
                    uint8_t statusA = 0U;
                    if (drv_asm330_getFsmStatus(&asm330, &statusA, NULL))
                    {
                        const bool crashEvent = (statusA & (0x01U << (IMU_FSM_CRASH_PROGRAM_NUMBER - 1U))) != 0U;
                        const bool impactEvent = (statusA & (0x01U << (IMU_FSM_IMPACT_PROGRAM_NUMBER - 1U))) != 0U;

                        if (imu.fsmCrashInitOk && crashEvent)
                        {
                            taskENTER_CRITICAL();
                            imu.fsmCrashEvent = true;
                            taskEXIT_CRITICAL();
                        }

                        if (imu.fsmImpactInitOk)
                        {
                            if (impactEvent || (imu_getAccelNormPeak() > IMU_IMPACT_THRESH_MPS))
                            {
                                if (!imu.fsmImpactActive)
                                {
                                    imu.impactAccelMax = imu.accelNormPeak;
                                }
                                else if (imu.accelNormPeak > imu.impactAccelMax)
                                {
                                    imu.impactAccelMax = imu.accelNormPeak;
                                }
                                imu.fsmImpactActive = true;
                            }
                            else
                            {
                                imu.fsmImpactActive = false;
                                imu.impactAccelMax = 0.0f;
                            }
                        }
                    }
                }
            }

            if (imuRan)
            {
                drv_timer_start(&imu.imuTimeout, IMU_TIMEOUT_MS);
                crashSensor_notifyFromImu();
            }

            const bool sleeping = app_vehicleState_sleeping();
            if (sleeping)
            {
                if (!wasSleeping)
                {
                    resetWakeDetector();
                }
                if (detectSleepWakeMovement())
                {
                    app_vehicleState_delaySleep(IMU_WAKE_DELAY_MS);
                }
            }
            else if (wasSleeping)
            {
                resetWakeDetector();
            }
            wasSleeping = sleeping;

            const bool imuTimeout = drv_timer_getState(&imu.imuTimeout) == DRV_TIMER_EXPIRED;
            const bool imuNotStarted = drv_timer_getState(&imu.imuTimeout) == DRV_TIMER_STOPPED;
            app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUOVERRUN, wasOverrun);
            app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUERROR, imuTimeout);
            app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUINVALID, imuTimeout || imuNotStarted || !imuCalibrated);
        }
        else
        {
            app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUOVERRUN, false);
            app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUERROR, true);
            app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUINVALID, true);
        }
        app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUSNA, false);
    }
    else
    {
        transitionImuState();
        app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUOVERRUN, false);
        app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUERROR, false);
        app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUSNA, true);
        app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUINVALID, true);
    }
}

/**
 * @brief  imu Module descriptor
 */
const ModuleDesc_S imu_desc = {
    .moduleInit        = &imu_init,
    .periodic100Hz_CLK = &imu100Hz_PRD,
};
