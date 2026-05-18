/**
 * @file SOC_Estimation.h
 * @brief  Header file for SOC Estimation
 */

#pragma once

#include "lib_interpolation.h"
#include "lib_linAlg.h"
#include "LIB_Types.h"

LIB_LINALG_DEFINE_N(socMatrix, float32_t, 3U);
typedef LIB_LINALG_INST_RMAT(socMatrix) soc_matrix_S;

LIB_LINALG_DEFINE_N(socVector, float32_t, 3U);
typedef LIB_LINALG_INST_CVEC(socVector) soc_col_vector_S;
typedef LIB_LINALG_INST_ROW(socVector) soc_row_vector_S;

typedef enum
{
    INIT = 0x00,
    INIT_VRC,
    RUNNING,
} battery_model_state_E;

typedef struct
{
    battery_model_state_E state;
    struct
    {
        float32_t                   Rnoise; // Measurement noise
        soc_matrix_S                Qnoise; // Process noise
        soc_matrix_S                Pinit;  // Covariance
        float32_t                   cellAH;
        float32_t                   minCellVoltage;
        float32_t                   maxCellVoltage;
        lib_interpolation_mapping_S * socMap;
        lib_interpolation_mapping_S * docvMap;
        lib_interpolation_mapping_S * RiMapDischarge;
        lib_interpolation_mapping_S * R1MapDischarge;
        lib_interpolation_mapping_S * C1MapDischarge;
        lib_interpolation_mapping_S * R2MapDischarge;
        lib_interpolation_mapping_S * C2MapDischarge;
        lib_interpolation_mapping_S * socMapCharge;
        lib_interpolation_mapping_S * RiMapCharge;
        lib_interpolation_mapping_S * R1MapCharge;
        lib_interpolation_mapping_S * C1MapCharge;
        lib_interpolation_mapping_S * R2MapCharge;
        lib_interpolation_mapping_S * C2MapCharge;
    } config;
    struct
    {
        float32_t initialCellVoltage;
        float32_t elapsedTime;
    }                init_vrc2;
    soc_col_vector_S X; // State Matrix {SOC, VRC1, VRC2}
    soc_matrix_S     P; // Covariance
    float32_t        cellVoltageSim;
    float32_t        dischargeLimit;
    float32_t        chargeLimit;
    soc_col_vector_S tmpVec;
    soc_col_vector_S tmpVec2;
    soc_matrix_S     eye3;
    soc_matrix_S     tmpMatrix;
    soc_matrix_S     tmpMatrix2;
} battery_model_S;    // keep structs lower case snake, variables camel

float32_t battery_model_get_SOC(battery_model_S* batteryModel);
float32_t battery_model_get_VRC1(battery_model_S* batteryModel);
float32_t battery_model_get_VRC2(battery_model_S* batteryModel);
void      battery_model_set_SOC(battery_model_S* batteryModel, float32_t soc);

void      battery_model_run(battery_model_S* batteryModel, float32_t cellVoltage, float32_t cellCurrent, float32_t minCellVoltage,
                            float32_t maxCellVoltage, float32_t dt);
void      battery_model_init(battery_model_S* batteryModel, float32_t soc);