/**
 * @file battery_model.c
 * @brief  Source code for SOC Estimation - RC battery model
 */

#include <string.h>

/**< Module headers */
#include "battery_model.h"
#include "BMS.h"

/**< Other Includes */
#include "lib_interpolation.h"


/******************************************************************************
 *                             Private Structs
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
 *                             Public Functions
 ******************************************************************************/

float32_t battery_model_get_SOC(battery_model_S* batteryModel)
{
    return batteryModel->X.elemCol[0];
}

float32_t battery_model_get_VRC1(battery_model_S* batteryModel)
{
    return batteryModel->X.elemCol[1];
}

float32_t battery_model_get_VRC2(battery_model_S* batteryModel)
{
    return batteryModel->X.elemCol[2];
}

/******************************************************************************
 *                             Private Functions
 ******************************************************************************/

static void cell_params_update(battery_model_S* batteryModel, float32_t cellCurrent, float32_t cellVoltage, cell_params_S* cellParams, float32_t SOC)
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
    if ((cellCurrent > 0) || (cellVoltage >= cellParams->OCV))
    {
        cellParams->Ri = (lib_interpolation_interpolate(batteryModel->config.RiMapCharge, SOC * 100));
        cellParams->R1 = (lib_interpolation_interpolate(batteryModel->config.R1MapCharge, SOC * 100));
        cellParams->C1 = (lib_interpolation_interpolate(batteryModel->config.C1MapCharge, SOC * 100));
        cellParams->R2 = (lib_interpolation_interpolate(batteryModel->config.R2MapCharge, SOC * 100));
        cellParams->C2 = (lib_interpolation_interpolate(batteryModel->config.C2MapCharge, SOC * 100));
    }
}

static void model_state_run(battery_model_S* batteryModel, float32_t cellVoltage, float32_t cellCurrent, float32_t dt)
{
    cell_params_S cellParams;

    cell_params_update(batteryModel, cellCurrent, cellVoltage, &cellParams, batteryModel->X.elemCol[0]);

    float32_t        tau1 = cellParams.R1 * cellParams.C1;
    float32_t        tau2 = cellParams.R2 * cellParams.C2;

    soc_matrix_S     A    = { { { 1.0f, 0.0f, 0.0f }, { 0.0f, expf(-dt / tau1), 0.0f }, { 0.0f, 0.0f, expf(-dt / tau2) } } };
    soc_col_vector_S B    = { { dt / (batteryModel->config.cellAH * 3600), -cellParams.R1 * (1 - expf(-dt / tau1)), -cellParams.R2 * (1 - expf(-dt / tau2)) } };

    // X = A*X + B*I(k);
    LIB_LINALG_MUL_RMATCVEC_SET(&A, &batteryModel->X, &batteryModel->tmpVec);
    LIB_LINALG_MUL_CVECSCALAR(&B, cellCurrent, &batteryModel->tmpVec2);
    LIB_LINALG_SUM_CVEC(&batteryModel->tmpVec, &batteryModel->tmpVec2, &batteryModel->X);

    // P = A*P*A' + Q_noise;
    LIB_LINALG_TRANSPOSE_MAT_GET(&A, &batteryModel->tmpMatrix);
    LIB_LINALG_MUL_RMATRMAT_SET(&batteryModel->P, &batteryModel->tmpMatrix,  &batteryModel->tmpMatrix2);
    LIB_LINALG_MUL_RMATRMAT_SET(&A,               &batteryModel->tmpMatrix2, &batteryModel->tmpMatrix);
    LIB_LINALG_SUM_MAT(&batteryModel->tmpMatrix, &batteryModel->config.Qnoise, &batteryModel->P);

    float32_t        dOCV   = lib_interpolation_interpolate(batteryModel->config.docvMap, batteryModel->X.elemCol[0] * 100);
    soc_row_vector_S H      = { { dOCV, -1, -1 } }; // jacobian vector

    batteryModel->cellVoltageSim = cellParams.OCV - batteryModel->X.elemCol[1] - batteryModel->X.elemCol[2] + cellCurrent * cellParams.Ri;
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

static void current_limit(battery_model_S* batteryModel, float32_t minCellVoltage, float32_t maxCellVoltage)
{
    float32_t RiDischarge = (lib_interpolation_interpolate(batteryModel->config.RiMapDischarge, battery_model_get_SOC(batteryModel) * 100));
    float32_t RiCharge    = (lib_interpolation_interpolate(batteryModel->config.RiMapCharge, battery_model_get_SOC(batteryModel) * 100));

    RiDischarge                  = RiDischarge * 1.2f; // safety factor find maximum deviation from average in cell testing
    RiCharge                     = RiCharge * 1.2f;    // safety factor find maximum deviation from average in cell testing

    batteryModel->dischargeLimit = (batteryModel->config.minCellVoltage - minCellVoltage) / RiDischarge;
    batteryModel->chargeLimit    = (batteryModel->config.maxCellVoltage - maxCellVoltage) / RiCharge;

    if (batteryModel->dischargeLimit > 0)
    {
        batteryModel->dischargeLimit = 0;
    }
    if (batteryModel->chargeLimit < 0)
    {
        batteryModel->chargeLimit = 0;
    }
}

/******************************************************************************
 *                             Public Functions
 ******************************************************************************/

void battery_model_init(battery_model_S* batteryModel, float32_t soc)
{
    batteryModel->state                        = INIT;
    batteryModel->init_vrc2.initialCellVoltage = 0;
    batteryModel->init_vrc2.elapsedTime        = 0;
    batteryModel->X                            = (soc_col_vector_S){ { soc, 0, 0 } };
    batteryModel->P                            = batteryModel->config.Pinit; // conifg
    batteryModel->cellVoltageSim               = 0;                          // conifg
    batteryModel->dischargeLimit               = 0;                          // using this for now, not sure how to initialise this
    batteryModel->chargeLimit                  = 0;
    batteryModel->tmpVec                       = (soc_col_vector_S){ 0 };
    batteryModel->tmpVec2                      = (soc_col_vector_S){ 0 };
    batteryModel->eye3                         = (soc_matrix_S){ 0 };
    batteryModel->tmpMatrix                    = (soc_matrix_S){ 0 };
    batteryModel->tmpMatrix2                   = (soc_matrix_S){ 0 };
}

// check input parameters
void battery_model_run(battery_model_S* batteryModel, float32_t cellVoltage, float32_t cellCurrent, float32_t minCellVoltage,
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
        model_state_run(batteryModel, cellVoltage, cellCurrent, dt);
        current_limit(batteryModel, minCellVoltage, maxCellVoltage);
    }
}
