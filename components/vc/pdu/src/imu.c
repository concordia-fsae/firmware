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
#include "lib_utility.h"
#include "app_vehicleState.h"
#include "drv_timer.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define IMU_IMPACT_THRESH_MPS (GRAVITY * 4)

#define IMU_WAKE_ACCEL_DELTA_MPS2 0.4f
#define IMU_WAKE_GYRO_DPS 3.0f
#define IMU_WAKE_HITS_REQUIRED 5U
#define IMU_WAKE_DELAY_MS (15U * 60000U)

#define IMU_TIMEOUT_MS 1000U
#define IMU_SELFTEST_TIMEOUT_MS 500U
#define BASELINE_SAMPLES 500U
#define MADGWICK_BETA 0.01f
#define IMU_YAW_CAL_MIN_TILT_DEG 5.0f

#define IMU_SELFTEST_ACCEL_MIN_MPS2 (GRAVITY * 0.040f)
#define IMU_SELFTEST_ACCEL_MAX_MPS2 (GRAVITY * 1.700f)
#define IMU_SELFTEST_GYRO_MIN_DPS   20.0f
#define IMU_SELFTEST_GYRO_MAX_DPS   80.0f

#define IMU_LPF_CUTOFF_HZ 100.0f
#define IMU_LPF_DT_S      0.01f

#define IMU_FSM_CRASH_PROGRAM_NUMBER    1U
#define IMU_FSM_IMPACT_PROGRAM_NUMBER   2U
#define IMU_FSM_ACTIVITY_PROGRAM_NUMBER 3U
#define IMU_FSM_CRASH_PROGRAM_SIZE      12U
#define IMU_FSM_IMPACT_PROGRAM_SIZE     12U
#define IMU_FSM_ACTIVITY_PROGRAM_SIZE   12U
#define IMU_FSM_ARM_CYCLES              10U

#define IMU_FSM_BASE_START_ADDR       0x0400U
#define IMU_FSM_CRASH_START_ADDR      IMU_FSM_BASE_START_ADDR
#define IMU_FSM_IMPACT_START_ADDR     (IMU_FSM_CRASH_START_ADDR + IMU_FSM_CRASH_PROGRAM_SIZE)
#define IMU_FSM_ACTIVITY_START_ADDR   (IMU_FSM_IMPACT_START_ADDR + IMU_FSM_IMPACT_PROGRAM_SIZE)

#define BUFFER_SAMPLE_SIZE 1000U

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
    INIT_SELFTEST,
    INIT_VEHICLEANGLE,
    STABILIZING,
    STABILIZING_VEHICLEANGLE,
    BASELINING,
    ZEROING,
    STABILIZING_FORWARD_TILT,
    ALIGNING_YAW,
    GET_VEHICLEANGLE,
    RUNNING,
} operatingMode_E;

typedef enum
{
    SELFTEST_STAGE_BASELINE = 0x00U,
    SELFTEST_STAGE_MEASURE,
} selfTestStage_E;

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
    bool            fsmActivityActive;
    float32_t       impactAccelMax;
    uint8_t         fsmArmCyclesLeft;
    bool            calibrating;
    bool            selfTesting;
    bool            selfTestFailed;
    bool            selfTestPassed;
    drv_timer_S     selfTestTimeout;
} imu;

static struct
{
    selfTestStage_E stage;
    drv_imu_accel_S cycleAccel;
    drv_imu_gyro_S  cycleGyro;
    uint16_t        cycleCountA;
    uint16_t        cycleCountG;
    drv_imu_accel_S accumAccel;
    drv_imu_gyro_S  accumGyro;
    uint16_t        accumCountA;
    uint16_t        accumCountG;
    drv_imu_accel_S baselineAccel;
    drv_imu_gyro_S  baselineGyro;
} selfTest = { 0 };

static LIB_BUFFER_FIFO_CREATE(imuBuffer, drv_asm330_fifoElement_S, BUFFER_SAMPLE_SIZE) = { 0 };

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

static const uint8_t imuFsmActivityProgram[IMU_FSM_IMPACT_PROGRAM_SIZE] = {
    0x50U, /* CONFIG_A: 1 threshold, 1 mask */
    0x00U, /* CONFIG_B */
    0x0CU, /* SIZE */
    0x00U, /* SETTINGS */
    0x00U, /* RESET_POINTER */
    0x00U, /* PROGRAM_POINTER */
    0x9aU, /* THRESH1 (LSB) = 1.15g */
    0x3cU, /* THRESH1 (MSB) */
    0x03U, /* MASKA: +/-V */
    0x00U, /* TMASKA */
    0x05U, /* NOP | GNTH1 */
    0x22U, /* CONTREL */
};

const drv_imu_vectorTransform_S rotationToVehicleFrame = {
    .rows = {
        { 0, -1, 0 },
        { -1, 0, 0 },
        { 0, 0, 1 },
    },
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void correctThenSetVector(drv_imu_vector_S* in, drv_imu_vector_S* zero, drv_imu_vector_S* out);
static void lpfAccel(drv_imu_accel_S* accel);
static void lpfGyro(drv_imu_gyro_S* gyro);
static void averageSamples(drv_imu_vector_S* vecA,
                           uint16_t* countA,
                           drv_imu_vector_S* vecG,
                           uint16_t* countG,
                           float32_t* accelNormPeak)
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
                if (accelNormPeak != NULL)
                {
                    drv_imu_vector_S corrected = { 0 };
                    float32_t peak = 0.0f;

                    correctThenSetVector(&tmp, &imuCalibration_data.zeroAccel, &corrected);
                    LIB_LINALG_GETNORM_CVEC(&corrected, &peak);
                    if (peak > *accelNormPeak)
                    {
                        *accelNormPeak = peak;
                    }
                }
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

static void resetSelfTestAccumulator(void)
{
    memset(&selfTest.accumAccel, 0, sizeof(selfTest.accumAccel));
    memset(&selfTest.accumGyro, 0, sizeof(selfTest.accumGyro));
    selfTest.accumCountA = 0U;
    selfTest.accumCountG = 0U;
    selfTest.cycleCountA = 0U;
    selfTest.cycleCountG = 0U;
}

static void accumulateSelfTestSamples(void)
{
    if (selfTest.cycleCountA > 0U)
    {
        selfTest.accumAccel.accelX += selfTest.cycleAccel.accelX * selfTest.cycleCountA;
        selfTest.accumAccel.accelY += selfTest.cycleAccel.accelY * selfTest.cycleCountA;
        selfTest.accumAccel.accelZ += selfTest.cycleAccel.accelZ * selfTest.cycleCountA;
        selfTest.accumCountA += selfTest.cycleCountA;
        selfTest.cycleCountA = 0U;
    }

    if (selfTest.cycleCountG > 0U)
    {
        selfTest.accumGyro.rotX += selfTest.cycleGyro.rotX * selfTest.cycleCountG;
        selfTest.accumGyro.rotY += selfTest.cycleGyro.rotY * selfTest.cycleCountG;
        selfTest.accumGyro.rotZ += selfTest.cycleGyro.rotZ * selfTest.cycleCountG;
        selfTest.accumCountG += selfTest.cycleCountG;
        selfTest.cycleCountG = 0U;
    }
}

static bool getSelfTestAverage(drv_imu_accel_S* accel, drv_imu_gyro_S* gyro)
{
    if ((selfTest.accumCountA <= BASELINE_SAMPLES) || (selfTest.accumCountG <= BASELINE_SAMPLES))
    {
        return false;
    }

    *accel = selfTest.accumAccel;
    accel->accelX /= selfTest.accumCountA;
    accel->accelY /= selfTest.accumCountA;
    accel->accelZ /= selfTest.accumCountA;

    *gyro = selfTest.accumGyro;
    gyro->rotX /= selfTest.accumCountG;
    gyro->rotY /= selfTest.accumCountG;
    gyro->rotZ /= selfTest.accumCountG;

    return true;
}

static void setAccelFromVector(const drv_imu_vector_S* in, drv_imu_accel_S* out)
{
    out->accelX = in->elemCol[0];
    out->accelY = in->elemCol[1];
    out->accelZ = in->elemCol[2];
}

static void setGyroFromVector(const drv_imu_vector_S* in, drv_imu_gyro_S* out)
{
    out->rotX = in->elemCol[0];
    out->rotY = in->elemCol[1];
    out->rotZ = in->elemCol[2];
}

static float32_t getAccelNorm(const drv_imu_accel_S* accel)
{
    return sqrtf((accel->accelX * accel->accelX) +
                 (accel->accelY * accel->accelY) +
                 (accel->accelZ * accel->accelZ));
}

static bool selfTestDeltaIsValid(float32_t delta, float32_t min, float32_t max)
{
    const float32_t absDelta = fabsf(delta);
    return (absDelta >= min) && (absDelta <= max);
}

static bool selfTestPassed(const drv_imu_accel_S* baselineAccel,
                           const drv_imu_gyro_S* baselineGyro,
                           const drv_imu_accel_S* selfTestAccel,
                           const drv_imu_gyro_S* selfTestGyro)
{
    return selfTestDeltaIsValid(selfTestAccel->accelX - baselineAccel->accelX,
                                IMU_SELFTEST_ACCEL_MIN_MPS2,
                                IMU_SELFTEST_ACCEL_MAX_MPS2) &&
           selfTestDeltaIsValid(selfTestAccel->accelY - baselineAccel->accelY,
                                IMU_SELFTEST_ACCEL_MIN_MPS2,
                                IMU_SELFTEST_ACCEL_MAX_MPS2) &&
           selfTestDeltaIsValid(selfTestAccel->accelZ - baselineAccel->accelZ,
                                IMU_SELFTEST_ACCEL_MIN_MPS2,
                                IMU_SELFTEST_ACCEL_MAX_MPS2) &&
           selfTestDeltaIsValid(selfTestGyro->rotX - baselineGyro->rotX,
                                IMU_SELFTEST_GYRO_MIN_DPS,
                                IMU_SELFTEST_GYRO_MAX_DPS) &&
           selfTestDeltaIsValid(selfTestGyro->rotY - baselineGyro->rotY,
                                IMU_SELFTEST_GYRO_MIN_DPS,
                                IMU_SELFTEST_GYRO_MAX_DPS) &&
           selfTestDeltaIsValid(selfTestGyro->rotZ - baselineGyro->rotZ,
                                IMU_SELFTEST_GYRO_MIN_DPS,
                                IMU_SELFTEST_GYRO_MAX_DPS);
}

static bool disregardSamples(void)
{
    static uint16_t countA = 0;
    static uint16_t countG = 0;
    drv_imu_vector_S tmp = {0};

    averageSamples(&tmp, &countA, &tmp, &countG, NULL);

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

    averageSamples(&tmp, &count, NULL, NULL, NULL);
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

static void rotateCalibrationByYaw(float32_t yawDeg)
{
    const float32_t yawRad = yawDeg * DEG_TO_RAD;
    drv_imu_vectorTransform_S yawRotation = { 0 };
    drv_imu_vectorTransform_S rotatedCalibration = { 0 };
    drv_imu_vector_S rotatedOffset = { 0 };

    LIB_LINALG_SETIDENTITY_RMAT(&yawRotation);
    yawRotation.rows[0][0] = cosf(yawRad);
    yawRotation.rows[0][1] = -sinf(yawRad);
    yawRotation.rows[1][0] = sinf(yawRad);
    yawRotation.rows[1][1] = cosf(yawRad);

    LIB_LINALG_MUL_RMATRMAT_SET(&yawRotation, &imuCalibration_data.rotation, &rotatedCalibration);
    imuCalibration_data.rotation = rotatedCalibration;

    LIB_LINALG_MUL_RMATCVEC_SET(&yawRotation, &imuCalibration_data.zeroAccel, &rotatedOffset);
    imuCalibration_data.zeroAccel = rotatedOffset;

    LIB_LINALG_MUL_RMATCVEC_SET(&yawRotation, &imuCalibration_data.zeroGyro, &rotatedOffset);
    imuCalibration_data.zeroGyro = rotatedOffset;
}

static bool calculateOffset(void)
{
    static drv_imu_vector_S sumA = {0};
    static uint16_t countA = 0;
    static drv_imu_vector_S sumG = {0};
    static uint16_t countG = 0;
    drv_imu_vector_S tmpA = {0};
    drv_imu_vector_S tmpG = {0};

    averageSamples(&tmpA, &countA, &tmpG, &countG, NULL);
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

static bool isForwardTilted(void)
{
    const float32_t minHorizontal = GRAVITY * sinf(IMU_YAW_CAL_MIN_TILT_DEG * DEG_TO_RAD);
    const float32_t horizontal = sqrtf((imu.accel.accelX * imu.accel.accelX) +
                                       (imu.accel.accelY * imu.accel.accelY));

    return (imu.accelNorm > (0.9f * GRAVITY)) &&
           (imu.accelNorm < (1.1f * GRAVITY)) &&
           (horizontal > minHorizontal);
}

static void updateMadgwick(lib_madgwick_S* f, lib_madgwick_euler_S* g, lib_madgwick_euler_S* a)
{
    const uint64_t currentTime = HW_TIM_getBaseTick();
    const float32_t dt = (float32_t)(currentTime - imu.lastCycle_us) / 1000000.0f;
    float32_t norm = 0.0f;

    // Dynamically reduce beta when accel magnitude deviates from 1g.
    LIB_LINALG_GETNORM_CVEC((drv_imu_vector_S*)a, &norm);
    const float32_t delta = fabsf(norm - GRAVITY) / GRAVITY;
    const float32_t bandStart = 0.05f; // full correction within +/-5%
    const float32_t bandEnd = 0.25f;   // min correction beyond +/-10%
    const float32_t minScale = 0.001f;  // keep some correction
    float32_t scale = 1.0f;
    if (delta > bandStart)
    {
        if (delta >= bandEnd)
        {
            scale = minScale;
        }
        else
        {
            const float32_t t = (delta - bandStart) / (bandEnd - bandStart);
            scale = 1.0f - t * (1.0f - minScale);
        }
    }

    const float32_t baseBeta = f->beta;
    f->beta = baseBeta * scale;
    madgwick_update_imu(f, g, a, dt);
    f->beta = baseBeta;
    imu.lastCycle_us = currentTime;
}

static bool calculateYawAlignment(void)
{
    static drv_imu_vector_S sum = {0};
    static uint16_t count = 0;
    drv_imu_vector_S tmp = {0};
    drv_imu_vector_S corrected = {0};

    averageSamples(&tmp, &count, NULL, NULL, NULL);
    LIB_LINALG_SUM_CVEC(&sum, &tmp, &sum);

    const bool valid = count > BASELINE_SAMPLES;

    if (valid)
    {
        LIB_LINALG_MUL_CVECSCALAR(&sum, (1.0f / count), &sum);
        correctThenSetVector(&sum, &imuCalibration_data.zeroAccel, &corrected);

        rotateCalibrationByYaw(-atan2f(corrected.elemCol[1], corrected.elemCol[0]) * RAD_TO_DEG);

        LIB_LINALG_CLEAR_CVEC(&sum);
        count = 0;
    }

    return valid;
}

static bool calculateVehicleAngle(void)
{
    static uint16_t countA = 0;
    static uint16_t countG = 0;
    static drv_imu_vector_S tmpA = {0};
    static drv_imu_vector_S tmpG = {0};

    averageSamples(&tmpA, &countA, &tmpG, &countG, NULL);

    const bool validG = (countG > BASELINE_SAMPLES);
    const bool validA = (countA > BASELINE_SAMPLES);
    if (!(validG && validA))
    {
        return false;
    }

    float32_t norm = 0;
    LIB_LINALG_GETNORM_CVEC(&tmpA, &norm);
    norm /= countA;

    if ((norm < (0.9f * GRAVITY)) || (norm > (1.1f * GRAVITY)))
    {
        return false;
    }

    if (countA)
    {
        LIB_LINALG_MUL_CVECSCALAR(&tmpA, (1.0f / countA), &tmpA);
        drv_imu_vector_S accelVec = { 0 };
        drv_imu_euler_S accelEuler = { 0 };

        correctThenSetVector(&tmpA, &imuCalibration_data.zeroAccel, &accelVec);
        setAccelFromVector(&accelVec, &imu.accel);
        imu.accelNorm = getAccelNorm(&imu.accel);
        accelEuler.x = imu.accel.accelX;
        accelEuler.y = imu.accel.accelY;
        accelEuler.z = imu.accel.accelZ;
        madgwick_init_quaternion_from_accel(&imu.madgwick, (lib_madgwick_euler_S*)&accelEuler);
        imu.lastCycle_us = HW_TIM_getBaseTick();
        taskENTER_CRITICAL();
        madgwick_get_euler_deg(&imu.madgwick, (drv_imu_euler_S*)&imu.vehicleAngle);
        taskEXIT_CRITICAL();
    }
    if (countG)
    {
        LIB_LINALG_MUL_CVECSCALAR(&tmpG, (1.0f / countG), &tmpG);
        drv_imu_vector_S gyroVec = { 0 };

        correctThenSetVector(&tmpG, &imuCalibration_data.zeroGyro, &gyroVec);
        setGyroFromVector(&gyroVec, &imu.gyro);
    }

    memset(&tmpA, 0x00U, sizeof(tmpA));
    memset(&tmpG, 0x00U, sizeof(tmpG));
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
    return drv_asm330_getFifoElements(&asm330,
                                      (uint8_t*)reserveStart,
                                      (uint16_t)(elements * sizeof(*reserveStart))) > 0U;
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

static bool handleImuSamples(void);
static void transitionImuState(void)
{
    switch (imu.operatingMode)
    {
        case INIT_SELFTEST:
        {
            const bool imuRan = egressFifo();
            const bool selfTestTimeout = drv_timer_getState(&imu.selfTestTimeout) == DRV_TIMER_EXPIRED;

            if (selfTestTimeout)
            {
                drv_timer_stop(&imu.selfTestTimeout);
                drv_asm330_stopSelfTest(&asm330);
                imu.selfTestFailed = true;
                imu.selfTesting = false;
                imu.operatingMode = INIT_VEHICLEANGLE;
            }
            else if (!imu.selfTesting)
            {
                if (imuRan && disregardSamples())
                {
                    resetSelfTestAccumulator();
                    selfTest.stage = SELFTEST_STAGE_BASELINE;
                    imu.selfTestFailed = false;
                    imu.selfTesting = true;
                    imu.selfTestPassed = false;
                }
            }
            else if (imuRan && handleImuSamples())
            {
                drv_imu_accel_S avgAccel = { 0 };
                drv_imu_gyro_S avgGyro = { 0 };

                accumulateSelfTestSamples();

                if (getSelfTestAverage(&avgAccel, &avgGyro))
                {
                    if (selfTest.stage == SELFTEST_STAGE_BASELINE)
                    {
                        selfTest.baselineAccel = avgAccel;
                        selfTest.baselineGyro = avgGyro;
                        resetSelfTestAccumulator();
                        selfTest.stage = SELFTEST_STAGE_MEASURE;
                        drv_asm330_startSelfTest(&asm330);
                        drv_timer_start(&imu.selfTestTimeout, IMU_SELFTEST_TIMEOUT_MS);
                    }
                    else
                    {
                        drv_timer_stop(&imu.selfTestTimeout);
                        drv_asm330_stopSelfTest(&asm330);
                        imu.selfTestFailed = !selfTestPassed(&selfTest.baselineAccel,
                                                             &selfTest.baselineGyro,
                                                             &avgAccel,
                                                             &avgGyro);
                        imu.selfTesting = false;
                        imu.selfTestPassed = !imu.selfTestFailed;
                        imu.operatingMode = INIT_VEHICLEANGLE;
                    }
                }
            }
            break;
        }
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
                imu.operatingMode = GET_VEHICLEANGLE;
                imu.calibrating = false;
            }
            egressFifo();
            break;
        case STABILIZING_FORWARD_TILT:
            if (disregardSamples())
            {
                imu.operatingMode = ALIGNING_YAW;
            }
            egressFifo();
            break;
        case ALIGNING_YAW:
            if (calculateYawAlignment())
            {
                lib_nvm_requestWrite(NVM_ENTRYID_IMU_CALIB);
                imu.operatingMode = GET_VEHICLEANGLE;
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

static bool handleImuSamples(void)
{
    if (((imu.operatingMode == RUNNING) || (imu.operatingMode == INIT_SELFTEST)) &&
        (drv_asm330_getState(&asm330) == DRV_ASM330_STATE_RUNNING))
    {
        drv_imu_vector_S sumA = {0};
        drv_imu_vector_S sumG = {0};
        uint16_t countA = 0;
        uint16_t countG = 0;
        float32_t accelNormPeak = 0.0f;

        averageSamples(&sumA, &countA, &sumG, &countG, &accelNormPeak);
        selfTest.cycleCountA = 0U;
        selfTest.cycleCountG = 0U;

        if (countA)
        {
            LIB_LINALG_MUL_CVECSCALAR(&sumA, (1.0f / countA), &sumA);
            drv_imu_vector_S accelVec = { 0 };
            drv_imu_accel_S accel = { 0 };

            correctThenSetVector(&sumA, &imuCalibration_data.zeroAccel, &accelVec);
            setAccelFromVector(&accelVec, &accel);
            selfTest.cycleAccel = accel;
            selfTest.cycleCountA = countA;
            imu.accelNormPeak = accelNormPeak;
            lpfAccel(&accel);
            imu.accelNorm = getAccelNorm(&accel);
            taskENTER_CRITICAL();
            memcpy(&imu.accel, &accel, sizeof(imu.accel));
            taskEXIT_CRITICAL();
        }
        if (countG)
        {
            LIB_LINALG_MUL_CVECSCALAR(&sumG, (1.0f / countG), &sumG);
            drv_imu_vector_S gyroVec = { 0 };
            drv_imu_gyro_S gyro = { 0 };

            correctThenSetVector(&sumG, &imuCalibration_data.zeroGyro, &gyroVec);
            setGyroFromVector(&gyroVec, &gyro);
            selfTest.cycleGyro = gyro;
            selfTest.cycleCountG = countG;
            lpfGyro(&gyro);
            taskENTER_CRITICAL();
            memcpy(&imu.gyro, &gyro, sizeof(imu.gyro));
            taskEXIT_CRITICAL();
        }

        if (imu.operatingMode == RUNNING)
        {
            drv_imu_euler_S gyroEuler = {
                .x = imu.gyro.rotX,
                .y = imu.gyro.rotY,
                .z = imu.gyro.rotZ,
            };
            drv_imu_euler_S accelEuler = {
                .x = imu.accel.accelX,
                .y = imu.accel.accelY,
                .z = imu.accel.accelZ,
            };

            updateMadgwick(&imu.madgwick, &gyroEuler, &accelEuler);

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

        return (countA > 0U) && (countG > 0U);
    }

    selfTest.cycleCountA = 0U;
    selfTest.cycleCountG = 0U;
    return false;
}

static bool getImuAlertStatus(void)
{
    bool ret = false;

    if (imu.fsmCrashInitOk && imu.fsmImpactInitOk)
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
                imu.fsmActivityActive = (statusA & (0x01U << (IMU_FSM_ACTIVITY_PROGRAM_NUMBER - 1U))) != 0U;
                ret = true;

                if (crashEvent)
                {
                    taskENTER_CRITICAL();
                    imu.fsmCrashEvent = true;
                    taskEXIT_CRITICAL();
                }

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

    return ret;
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

bool imu_getSelfTestPassed(void)
{
    return imu.selfTestPassed;
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

bool imu_isSelfTesting(void)
{
    return imu.operatingMode == INIT_SELFTEST;
}

bool imu_isYawCalibrating(void)
{
    return (imu.operatingMode == STABILIZING_FORWARD_TILT) ||
           (imu.operatingMode == ALIGNING_YAW);
}

/**
 * @brief  imu Module Init function
 */
static void imu_init()
{
    drv_asm330_init(&asm330);
    drv_timer_init(&imu.imuTimeout);
    drv_timer_init(&imu.selfTestTimeout);
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

    imu.operatingMode = INIT_SELFTEST;

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
    drv_asm330_loadFsmProgram(&asm330,
                              IMU_FSM_ACTIVITY_PROGRAM_NUMBER,
                              IMU_FSM_ACTIVITY_START_ADDR,
                              imuFsmActivityProgram,
                              IMU_FSM_ACTIVITY_PROGRAM_SIZE,
                              ASM330LHB_ODR_FSM_104Hz);
    imu.fsmCrashEvent = false;
    imu.fsmImpactActive = false;
    imu.impactAccelMax = 0.0f;
    imu.fsmArmCyclesLeft = IMU_FSM_ARM_CYCLES;
    imu.selfTesting = false;
    imu.selfTestFailed = false;
    selfTest.stage = SELFTEST_STAGE_BASELINE;
    resetSelfTestAccumulator();
    app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUFSMINITFAILURE,
                                   !(imu.fsmCrashInitOk && imu.fsmImpactInitOk));
}

/**
 * @brief  imu Module 100Hz periodic function
 */
static void imu100Hz_PRD(void)
{
    const bool imuCalibrated = memcmp(&imuCalibration_data, &imuCalibration_default, sizeof(imuCalibration_data));
    const bool wasOverrun = drv_asm330_getFifoOverrun(&asm330);
    const bool gotAlerts = getImuAlertStatus();
    const bool imuDataAvailable = (imu.operatingMode == RUNNING) || imu.selfTesting;

    app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUOVERRUN, wasOverrun);
    app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUUNCALIBRATED, !imuCalibrated);
    app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUYAWCALIBRATING, imu_isYawCalibrating());
    app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUYAWCALIBRATIONFAILED, false);

    if (gotAlerts)
    {
        crashSensor_notifyFromImu();
        if (imu.fsmActivityActive)
        {
            app_vehicleState_delaySleep(IMU_WAKE_DELAY_MS);
        }
    }

    if (imu.operatingMode == RUNNING)
    {
        CAN_digitalStatus_E tmp = CAN_DIGITALSTATUS_SNA;
        const bool calibrate = (CANRX_get_signal(VEH, SWS_requestCalibImu, &tmp) == CANRX_MESSAGE_VALID) &&
                               (tmp == CAN_DIGITALSTATUS_ON);
        const bool yawCalibrate = (CANRX_get_signal(VEH, SWS_requestCalibImuYaw, &tmp) == CANRX_MESSAGE_VALID) &&
                                  (tmp == CAN_DIGITALSTATUS_ON);
        const bool imuSelfTestRequest = (CANRX_get_signal(VEH, SWS_requestImuSelfTest, &tmp) == CANRX_MESSAGE_VALID) &&
                                        (tmp == CAN_DIGITALSTATUS_ON);

        if (calibrate)
        {
            lib_nvm_clearEntry(NVM_ENTRYID_IMU_CALIB);
            imu.operatingMode = INIT;
            imu.calibrating = true;
        }
        else if (imuSelfTestRequest)
        {
            drv_timer_stop(&imu.selfTestTimeout);
            imu.selfTestPassed = false;
            imu.operatingMode = INIT_SELFTEST;
            imu.selfTesting = false;
        }
        else if (yawCalibrate && imuCalibrated)
        {
            if (isForwardTilted())
            {
                imu.operatingMode = STABILIZING_FORWARD_TILT;
                imu.calibrating = true;
            }
            else
            {
                app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUYAWCALIBRATIONFAILED, true);
            }
        }
        else if ((drv_asm330_getState(&asm330) == DRV_ASM330_STATE_RUNNING))
        {
            const bool imuRan = egressFifo();
            if (imuRan)
            {
                (void)handleImuSamples();
            }

            if (imuRan)
            {
                drv_timer_start(&imu.imuTimeout, IMU_TIMEOUT_MS);
            }

            const bool imuTimeout = drv_timer_getState(&imu.imuTimeout) == DRV_TIMER_EXPIRED;
            const bool imuNotStarted = drv_timer_getState(&imu.imuTimeout) == DRV_TIMER_STOPPED;
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
        app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUSNA, !imuDataAvailable);
        app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUINVALID, !imuDataAvailable);
    }
    app_faultManager_setFaultState(FM_FAULT_VCPDU_IMUSELFTESTFAILED, imu.selfTestFailed);
}

/**
 * @brief  imu Module descriptor
 */
const ModuleDesc_S imu_desc = {
    .moduleInit        = &imu_init,
    .periodic100Hz_CLK = &imu100Hz_PRD,
};
