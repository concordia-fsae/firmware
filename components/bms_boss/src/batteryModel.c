/**
 * @file batteryModel.c
 * @brief  Source code for SOC Estimation - RC battery model
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include <string.h>

/**< Module headers */
#include "batteryModel.h"
#include "BMS.h"

/**< Other Includes */
#include "lib_interpolation.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define  RI_SAFETY_FACTOR              1.0f
#define  DRIVETIME_AVG_WINDOW_S        60
#define  DRIVETIME_MIN_SOC             0.01
#define  DRIVETIME_MODEL_TIMESTEP_S    10

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t OCV;
    float32_t SOC;
    float32_t Ri;
    float32_t R1;
    float32_t R2;
    float32_t C1;
    float32_t C2;
} cell_params_S;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void cell_params_update(batteryModel_S* batteryModel, float32_t cellCurrent, float32_t cellVoltage, cell_params_S* cellParams, float32_t SOC)
{
    cellParams->SOC = SOC;
    cellParams->OCV = (lib_interpolation_interpolate(batteryModel->config.socMap, SOC * 100));

    if ((cellCurrent <= 0) || (cellVoltage <= cellParams->OCV))
    {
        cellParams->Ri = (lib_interpolation_interpolate(batteryModel->config.RiMapDischarge, SOC * 100));
        cellParams->R1 = (lib_interpolation_interpolate(batteryModel->config.R1MapDischarge, SOC * 100));
        cellParams->C1 = (lib_interpolation_interpolate(batteryModel->config.C1MapDischarge, SOC * 100));
        cellParams->R2 = (lib_interpolation_interpolate(batteryModel->config.R2MapDischarge, SOC * 100));
        cellParams->C2 = (lib_interpolation_interpolate(batteryModel->config.C2MapDischarge, SOC * 100));
    }
    else if ((cellCurrent > 0) || (cellVoltage > cellParams->OCV))
    {
        cellParams->Ri = (lib_interpolation_interpolate(batteryModel->config.RiMapCharge, SOC * 100));
        cellParams->R1 = (lib_interpolation_interpolate(batteryModel->config.R1MapCharge, SOC * 100));
        cellParams->C1 = (lib_interpolation_interpolate(batteryModel->config.C1MapCharge, SOC * 100));
        cellParams->R2 = (lib_interpolation_interpolate(batteryModel->config.R2MapCharge, SOC * 100));
        cellParams->C2 = (lib_interpolation_interpolate(batteryModel->config.C2MapCharge, SOC * 100));
    }
}

static void model_states_run(batteryModel_S* batteryModel, float32_t cellVoltage, float32_t cellCurrent, float32_t dt)
{
    cell_params_S cellParams;

    cell_params_update(batteryModel, cellCurrent, cellVoltage, &cellParams, batteryModel->X.elemCol[0]);

    float32_t tau1 = cellParams.R1 * cellParams.C1;
    float32_t tau2 = cellParams.R2 * cellParams.C2;

    batteryModel->A = (soc_matrix_S){ { { 1.0f, 0.0f, 0.0f }, { 0.0f, expf(-dt / tau1), 0.0f }, { 0.0f, 0.0f, expf(-dt / tau2) } } };
    batteryModel->B = (soc_col_vector_S){ { dt / (batteryModel->config.cellAH * 3600), -cellParams.R1 * (1 - expf(-dt / tau1)), -cellParams.R2 * (1 - expf(-dt / tau2)) } };

    // X = A*X + B*I(k);
    LIB_LINALG_MUL_RMATCVEC_SET(&batteryModel->A, &batteryModel->X, &batteryModel->tmpVec);
    LIB_LINALG_MUL_CVECSCALAR(&batteryModel->B, cellCurrent, &batteryModel->tmpVec2);
    LIB_LINALG_SUM_CVEC(&batteryModel->tmpVec, &batteryModel->tmpVec2, &batteryModel->X);

    batteryModel->cellVoltageSim = cellParams.OCV - batteryModel->X.elemCol[1] - batteryModel->X.elemCol[2] + cellCurrent * cellParams.Ri;
}

static void model_prediction_run(batteryModel_S* batteryModel, float32_t cellVoltage, float32_t cellCurrent, float32_t dt)
{
    cell_params_S cellParams;

    cell_params_update(batteryModel, cellCurrent, cellVoltage, &cellParams, batteryModel->X.elemCol[0]);

    model_states_run(batteryModel, cellVoltage, cellCurrent, dt);

    // P = A*P*A' + Q_noise;
    LIB_LINALG_TRANSPOSE_MAT_GET(&batteryModel->A, &batteryModel->tmpMatrix);
    LIB_LINALG_MUL_RMATRMAT_SET(&batteryModel->P, &batteryModel->tmpMatrix,  &batteryModel->tmpMatrix2);
    LIB_LINALG_MUL_RMATRMAT_SET(&batteryModel->A, &batteryModel->tmpMatrix2, &batteryModel->tmpMatrix);
    LIB_LINALG_SUM_MAT(&batteryModel->tmpMatrix, &batteryModel->config.Qnoise, &batteryModel->P);

    float32_t        dOCV   = lib_interpolation_interpolate(batteryModel->config.docvMap, batteryModel->X.elemCol[0] * 100);
    soc_row_vector_S H      = { { dOCV, -1, -1 } }; // jacobian vector

    float32_t        error  = cellVoltage - batteryModel->cellVoltageSim;

    // K = P*H'*(H*P*H' + R_noise)^-1;
    float32_t        scalar = 0;
    LIB_LINALG_TRANSPOSE_RVEC_GET(&H, &batteryModel->tmpVec);
    LIB_LINALG_MUL_RMATCVEC_SET(&batteryModel->P, &batteryModel->tmpVec, &batteryModel->tmpVec2);
    LIB_LINALG_MUL_RVECCVEC_SET(&H, &batteryModel->tmpVec2, &scalar);
    scalar = 1 / (scalar + batteryModel->config.Rnoise);
    soc_col_vector_S K = { 0 };    // Kalman Gain
    LIB_LINALG_TRANSPOSE_RVEC_GET(&H, &batteryModel->tmpVec);
    LIB_LINALG_MUL_RMATCVEC_SET(&batteryModel->P, &batteryModel->tmpVec, &batteryModel->tmpVec2);
    LIB_LINALG_MUL_CVECSCALAR(&batteryModel->tmpVec2, scalar, &K);

    // X = X+K1*error;
    LIB_LINALG_MUL_CVECSCALAR(&K, error, &batteryModel->tmpVec);
    LIB_LINALG_SUM_CVEC(&batteryModel->X, &batteryModel->tmpVec, &batteryModel->X);

    // P = (eye(3,3) - K1*H)*P;
    LIB_LINALG_MUL_CVECRVEC(&K, &H, &batteryModel->tmpMatrix);
    LIB_LINALG_SETIDENTITY_RMAT(&batteryModel->eye3);
    LIB_LINALG_DIF_MAT(&batteryModel->eye3, &batteryModel->tmpMatrix, &batteryModel->tmpMatrix2);
    LIB_LINALG_MUL_RMATRMAT_SET(&batteryModel->tmpMatrix2, &batteryModel->P, &batteryModel->tmpMatrix);
    batteryModel->P = batteryModel->tmpMatrix;

    if (batteryModel->X.elemCol[0] > 1.0f)
    {
        batteryModel->X.elemCol[0] = 1.0f;
    }
    if (batteryModel->X.elemCol[0] < 0.0f)
    {
        batteryModel->X.elemCol[0] = 0.0f;
    }
}

static void current_limit(batteryModel_S* batteryModel, float32_t minCellVoltage, float32_t maxCellVoltage, float32_t cellCurrent)
{
    float32_t RiDischarge = (lib_interpolation_interpolate(batteryModel->config.RiMapDischarge, batteryModel_getSOC(batteryModel) * 100));
    float32_t RiCharge    = (lib_interpolation_interpolate(batteryModel->config.RiMapCharge, batteryModel_getSOC(batteryModel) * 100));

    RiDischarge                  = RiDischarge * RI_SAFETY_FACTOR;
    RiCharge                     = RiCharge * RI_SAFETY_FACTOR;

    batteryModel->dischargeLimit = (batteryModel->config.minCellVoltage - minCellVoltage) / RiDischarge + cellCurrent;
    batteryModel->chargeLimit    = (batteryModel->config.maxCellVoltage - maxCellVoltage) / RiCharge + cellCurrent;

    if (batteryModel->dischargeLimit > 0)
    {
        batteryModel->dischargeLimit = 0;
    }
    if (batteryModel->chargeLimit < 0)
    {
        batteryModel->chargeLimit = 0;
    }
}

static void driveTime_remaining(batteryModel_S* batteryModel, float32_t cellVoltage, float32_t avgPower)
{
    int            numIterations   = 0;

    batteryModel->driveTimeRemaining = 0;
    batteryModel_S batteryModelTmp = *batteryModel;
    batteryModelTmp.cellVoltageSim   = cellVoltage;
    while (batteryModel_getSOC(&batteryModelTmp) > DRIVETIME_MIN_SOC && batteryModel->driveTimeRemaining <= 30 * 60)
    {
        model_states_run(&batteryModelTmp, cellVoltage, avgPower / batteryModelTmp.cellVoltageSim, DRIVETIME_MODEL_TIMESTEP_S);
        batteryModel->driveTimeRemaining = batteryModel->driveTimeRemaining + DRIVETIME_MODEL_TIMESTEP_S;
        numIterations++;
    }
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t batteryModel_getSOC(batteryModel_S* batteryModel)
{
    return batteryModel->X.elemCol[0];
}

float32_t batteryModel_getVRC1(batteryModel_S* batteryModel)
{
    return batteryModel->X.elemCol[1];
}

float32_t batteryModel_getVRC2(batteryModel_S* batteryModel)
{
    return batteryModel->X.elemCol[2];
}

void batteryModel_setSOC(batteryModel_S* batteryModel, float32_t soc)
{
    batteryModel->X.elemCol[0] = soc;
}


void batteryModel_init(batteryModel_S* batteryModel, float32_t soc)
{
    batteryModel->state                        = INIT;
    batteryModel->init_vrc2.initialCellVoltage = 0;
    batteryModel->init_vrc2.elapsedTime        = 0;
    batteryModel->driveTimeRemaining           = 0;
    batteryModel->avgPower                     = 0;
    batteryModel->avgTime                      = 0;
    batteryModel->X                            = (soc_col_vector_S){ { soc, 0, 0 } };
    batteryModel->A                            = (soc_matrix_S){ { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } };
    batteryModel->B                            = (soc_col_vector_S){ { 0.0f, 0.0f, 0.0f } };
    batteryModel->P                            = batteryModel->config.Pinit; // conifg
    batteryModel->cellVoltageSim               = 0;                          // conifg
    batteryModel->dischargeLimit               = 0;
    batteryModel->chargeLimit                  = 0;
    batteryModel->tmpVec                       = (soc_col_vector_S){ 0 };
    batteryModel->tmpVec2                      = (soc_col_vector_S){ 0 };
    batteryModel->eye3                         = (soc_matrix_S){ 0 };
    batteryModel->tmpMatrix                    = (soc_matrix_S){ 0 };
    batteryModel->tmpMatrix2                   = (soc_matrix_S){ 0 };
}

// check input parameters
void batteryModel_run(batteryModel_S* batteryModel, float32_t cellVoltage, float32_t cellCurrent, float32_t minCellVoltage,
                      float32_t maxCellVoltage, float32_t dt)
{
    if (batteryModel->state == INIT)
    {
        batteryModel->init_vrc2.initialCellVoltage = cellVoltage;
        batteryModel->state                        = INIT_VRC;
    }
    else if ((batteryModel->state == INIT_VRC) && (batteryModel->init_vrc2.elapsedTime < 1))    // wait 1s to find VRC states
    {
        batteryModel->init_vrc2.elapsedTime += dt;
    }
    else if ((batteryModel->state == INIT_VRC) && (batteryModel->init_vrc2.elapsedTime >= 1))
    {
        float32_t     t = batteryModel->init_vrc2.elapsedTime;

        cell_params_S cellParams;
        cell_params_update(batteryModel, cellCurrent, cellVoltage, &cellParams, batteryModel->X.elemCol[0]);
        float32_t     tau1 = cellParams.R1 * cellParams.C1; float32_t tau2 = cellParams.R2 * cellParams.C2;

        // Y matrix
        float32_t     y0  = cellParams.OCV - batteryModel->init_vrc2.initialCellVoltage;
        float32_t     y1  = cellParams.OCV - cellVoltage;

        // A = [1 1; e^-t/tau1 e^-t/tau2]
        float32_t     a11 = 1.0f;
        float32_t     a12 = 1.0f;
        float32_t     a21 = expf(-t / tau1);
        float32_t     a22 = expf(-t / tau2);

        float32_t     det = a11 * a22 - a12 * a21;

        if (det > 1e-6f)
        {
            // X = A^-1 * Y
            batteryModel->X.elemCol[1] = (y0 * a22 - a12 * y1) / det;
            batteryModel->X.elemCol[2] = (-y0 * a21 + a11 * y1) / det;
        }
        else
        {
            batteryModel->X.elemCol[1] = 0;
            batteryModel->X.elemCol[2] = 0;
        }
        batteryModel->state = RUNNING;
    }
    else if (batteryModel->state == RUNNING)
    {
        model_prediction_run(batteryModel, cellVoltage, cellCurrent, dt);
        current_limit(batteryModel, minCellVoltage, maxCellVoltage, cellCurrent);

        batteryModel->avgTime  = batteryModel->avgTime + dt;
        batteryModel->avgPower = batteryModel->avgPower + cellCurrent * cellVoltage * dt;
        if (batteryModel->avgTime >= DRIVETIME_AVG_WINDOW_S)
        {
            batteryModel->avgPower = batteryModel->avgPower / batteryModel->avgTime;
            driveTime_remaining(batteryModel, cellVoltage, batteryModel->avgPower);
            batteryModel->avgPower = 0;
            batteryModel->avgTime  = 0;
        }
    }
}
