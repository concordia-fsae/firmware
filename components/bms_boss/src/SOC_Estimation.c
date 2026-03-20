/**
 * @file SOC_Estimation.c
 * @brief  Source code for SOC Estimation
 */

 //**notes. Add failsafe incase current sensor breaks mid run */

/**< Module header */
#include "SOC_Estimation.h"
#include "BMS.h"
/**< Driver Includes */

/**< Other Includes */
#include "string.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#include "lib_interpolation.h"

#include "LIB_Types.h"
#include "lib_linAlg.h"

#define MAX_LINE 256
#define MAX_COLS 10

#define Total_packAH 18.0f
#define CELL_AH 4.5f
#define Max_voltage 4.20f
#define Min_OCV 2.50f
#define Min_voltage 2.20f
#define Resistance_offset 0.0f

#define R_noise 0.05f


BMSB_S BMS;

//SOC lookup
const float32_t SOC_start = 100; //this must come from nvm

LIB_LINALG_DEFINE_N(socMatrix, float32_t, 2U);
typedef LIB_LINALG_INST_RMAT(socMatrix) soc_matrix_S;

LIB_LINALG_DEFINE_N(socVector, float32_t, 2U);
typedef LIB_LINALG_INST_CVEC(socVector) soc_col_vector_S;
typedef LIB_LINALG_INST_ROW(socVector) soc_row_vector_S;

//static soc_matrix_S Q_noise =  { {{7e-8, 0, 0}, {0, 6e-5, 0}, {0, 0, 6e-5}} }; //Process noise
//static soc_matrix_S P = { {{1e-4, 0, 0}, {0, 1e-4, 0}, {0, 0, 1e-4}} }; //Covariance

static soc_matrix_S Q_noise =  { {{7e-8, 0}, {0, 6e-5}} }; //Process noise
static soc_matrix_S P = { {{1e-4, 0}, {0, 1e-4}} }; //Covariance


//static soc_col_vector_S X =  {SOC_start, 0, 0}; //state //soc_start is a placeholder for nvm soc at startup
static soc_col_vector_S X =  {SOC_start/100, 0}; //state //soc_start is a placeholder for nvm soc at startup
//^ all logi is in soc 0-1 not 0-100

/******************************************************************************
 *                             Interpolations Maps
 ******************************************************************************/

static lib_interpolation_point_S SOC_OCVMap[] = {
    {.x = 0.0f,  .y = 2.42f},   //SOC to OCV
    {.x = 10.0f, .y = 3.178f},
    {.x = 20.0f, .y = 3.37f},
    {.x = 30.0f, .y = 3.52f},
    {.x = 40.0f, .y = 3.62f},
    {.x = 50.0f, .y = 3.75f},
    {.x = 60.0f, .y = 3.84f},
    {.x = 70.0f, .y = 3.94f},
    {.x = 80.0f, .y = 4.05f},
    {.x = 90.0f, .y = 4.09f},
    {.x = 100.0f, .y = 4.20f},
};

static lib_interpolation_mapping_S SOC_OCV_Func = {
    .points = (lib_interpolation_point_S*)&SOC_OCVMap,
    .number_points = COUNTOF(SOC_OCVMap),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_point_S SOC_dOCVMap[] = {
    {.x = 0.0f, .y = 7.58f}, //SOC to dOCV
    {.x = 10.0f, .y = 4.75f},
    {.x = 20.0f, .y = 1.71f},
    {.x = 30.0f, .y = 1.25f},
    {.x = 40.0f, .y = 1.15f},
    {.x = 50.0f, .y = 1.1f},
    {.x = 60.0f, .y = 0.95f},
    {.x = 70.0f, .y = 1.05f},
    {.x = 80.0f, .y = 0.75f},
    {.x = 90.0f, .y = 0.75f},
    {.x = 100.0f, .y = 1.1f},
};

static lib_interpolation_mapping_S SOC_dOCV_Func = {
    .points = (lib_interpolation_point_S*)&SOC_dOCVMap,
    .number_points = COUNTOF(SOC_dOCVMap),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_point_S SOC_RiMap[] = {
    {.x = 0.0f, .y = 0.0074 + Resistance_offset}, //SOC to Ri
    {.x = 10.0f, .y = 0.0074 + Resistance_offset},
    {.x = 20.0f, .y = 0.0068 + Resistance_offset},
    {.x = 30.0f, .y = 0.0063 + Resistance_offset},
    {.x = 40.0f, .y = 0.0062 + Resistance_offset},
    {.x = 50.0f, .y = 0.0062 + Resistance_offset},
    {.x = 60.0f, .y = 0.0054+ Resistance_offset},
    {.x = 70.0f, .y = 0.0056 + Resistance_offset},
    {.x = 80.0f, .y = 0.0065 + Resistance_offset},
    {.x = 90.0f, .y = 0.0066 + Resistance_offset},
    {.x = 100.0f, .y = 0.0079 + Resistance_offset},
};

static lib_interpolation_mapping_S SOC_Ri_FUNC = {
    .points = (lib_interpolation_point_S*)&SOC_RiMap,
    .number_points = COUNTOF(SOC_RiMap),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_point_S SOC_R1Map[] = {
    {.x = 0.0f, .y = 0.0049}, //SOC to R1
    {.x = 10.0f, .y = 0.0049f},
    {.x = 20.0f, .y = 0.0032f},
    {.x = 30.0f, .y = 0.002f},
    {.x = 40.0f, .y = 0.0019f},
    {.x = 50.0f, .y = 0.0019f},
    {.x = 60.0f, .y = 0.0021f},
    {.x = 70.0f, .y = 0.0024f},
    {.x = 80.0f, .y = 0.0028f},
    {.x = 90.0f, .y = 0.0023f},
    {.x = 100.0f, .y = 0.0045f},
};

static lib_interpolation_mapping_S SOC_R1_FUNC = {
    .points = (lib_interpolation_point_S*)&SOC_R1Map,
    .number_points = COUNTOF(SOC_R1Map),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_point_S SOC_C1Map[] = {
    {.x = 0.0f, .y = 697.3229f}, //SOC to C1
    {.x = 10.0f, .y = 697.3229f},
    {.x = 20.0f, .y = 1748.00f},
    {.x = 30.0f, .y = 1935.5f},
    {.x = 40.0f, .y = 1587.1f},
    {.x = 50.0f, .y = 1456.5f},
    {.x = 60.0f, .y = 447.43f},
    {.x = 70.0f, .y = 671.78f},
    {.x = 80.0f, .y = 1376.00f},
    {.x = 90.0f, .y = 1266.2f},
    {.x = 100.0f, .y = 625.55f},
};

static lib_interpolation_mapping_S SOC_C1_FUNC = {
    .points = (lib_interpolation_point_S*)&SOC_C1Map,
    .number_points = COUNTOF(SOC_C1Map),
    .saturate_left = true,
    .saturate_right = true,
};

/******************************************************************************
 *                             Private Functions
 ******************************************************************************/

static void SOC_RC_update(void){
    Cell_param.Ri = (lib_interpolation_interpolate(&SOC_Ri_FUNC, Cell_param.SOC));
    Cell_param.R1 = (lib_interpolation_interpolate(&SOC_R1_FUNC, Cell_param.SOC));
    Cell_param.C1 = (lib_interpolation_interpolate(&SOC_C1_FUNC, Cell_param.SOC));
}

static void SOC_updateOCV(void){
    Cell_param.OCV = (lib_interpolation_interpolate(&SOC_OCV_Func, Cell_param.SOC));
}

static void init_SOC()
{
    //Cell_param.SOC = (current_data.pack_amp_hours / Total_packAH) * 100.0f;
    //**Circuit_param.last_step_us = HW_TIM_getBaseTick();
}


/******************************************************************************
 *                             Public Functions
 ******************************************************************************/

 void socEstimation_init(void){
    Cell_param.voltage =0;
    Cell_param.Ri = 0;
    Cell_param.R1 = 0;
    Cell_param.C1 = 0;
    Cell_param.OCV = 0;
    Circuit_param.VRC1 = 0;
    Circuit_param.dVRC1 = 0;
    Circuit_param.current = 0;
    Circuit_param.VR1 = 0;
    init_SOC();
 }

 //check input parameters
void SOCestimation(){
    // get all circuit parameters
    
    SOC_RC_update();
    SOC_updateOCV();

    FILE *file = fopen("EnduranceResults.csv", "r");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    char line[MAX_LINE];
    
    //**onst uint64_t this_step = HW_TIM_getBaseTick();
    //**const uint32_t dt = (uint32_t)(this_step - Circuit_param.last_step_us)/1000000;
    int dt = 1;
    //float32_t dt = (float32_t)(*dt_us) * 0.000001f;

    float32_t tao1 = Cell_param.R1*Cell_param.C1; 
    //float32_t tao2 = Cell_param.R2*Cell_param.C2;

    //coulomb count
    //Cell_param.SOC = Cell_param.SOC - ((BMS.pack_current*dt) / (CELL_AH*3600))*100;

    Circuit_param.VRC1 = ((exp(-dt/tao1))*X.elemCol[2] + Cell_param.R1*(1-exp(-dt/tao1))*BMS.pack_current);
    //Circuit_param.Vrc2 = ((exp(-dt/tao2))*Circuit_param.Vrc2 + Circuit_param.R2*(1-exp(-dt/tao2))*I(k));

    Cell_param.voltage = Cell_param.OCV - Circuit_param.VRC1 - BMS.pack_current*Cell_param.Ri;

    //soc_matrix_S A = { {{1, 0, 0}, {0, exp(-dt/tao1), 0}, {0, 0, exp(-dt/tao2)}} };
    //soc_col_vector_S B = {-dt/(CELL_AH*3600), Cell_param.R1*(1-exp(-dt/tao1)), Cell_param.R2*(1-exp(-dt/tao2))}
    soc_matrix_S A = { {{1, 0}, {0, exp(-dt/tao1)}} }; //process matrix
    soc_col_vector_S B = {-dt/(CELL_AH*3600), Cell_param.R1*(1-exp(-dt/tao1))}; //input vector

    soc_col_vector_S temp_vec =  {0};
    soc_col_vector_S temp_vec2 =  {0};
    soc_matrix_S eye3 =  {0};
    soc_matrix_S temp_matrix =  {0};
    soc_matrix_S temp_matrix2 =  {0};

    //X = A*X + B*I(k); 
    LIB_LINALG_MUL_RMATCVEC(&A, &X, &temp_vec);
    LIB_LINALG_MUL_CVECSCALAR(&B, BMS.pack_current, &temp_vec2);
    LIB_LINALG_SUM_CVEC(&temp_vec, &temp_vec2, &X);

    //P = A*P*A' + Q_noise;
    LIB_LINALG_TRANSPOSE_MAT_GET(&A, &temp_matrix);
    LIB_LINALG_MUL_RMATRMAT_SET(&P, &temp_matrix, &temp_matrix2);
    LIB_LINALG_MUL_RMATRMAT_SET(&A, &temp_matrix2, &temp_matrix);
    LIB_LINALG_SUM_MAT(&temp_matrix, &Q_noise, &P);

    float32_t dOCV = lib_interpolation_interpolate(&SOC_dOCV_Func, Cell_param.SOC); //put this in a struct or smthg
    soc_row_vector_S H = {dOCV, -1, -1}; //jacobian vector

    float32_t error = BMS.pack_voltage_measured; //measured vs caluclated??
    float32_t scalar = 0;

    //K1 = P*H'*(H*P*H' + R_noise)^-1;
    LIB_LINALG_TRANSPOSE_RVEC_GET(&H, &temp_vec);
    LIB_LINALG_MUL_RMATCVEC(&P, &temp_vec, &temp_vec2);
    LIB_LINALG_MUL_RVECCVEC(&H, &temp_vec2, &scalar);

    scalar = 1/(scalar + R_noise);

    soc_col_vector_S K1 =  {0}; //Kalman Gain
    LIB_LINALG_TRANSPOSE_RVEC_GET(&H, &temp_vec);
    LIB_LINALG_MUL_RMATCVEC(&P, &temp_vec, &temp_vec2);
    LIB_LINALG_MUL_CVECSCALAR(&temp_vec2, scalar, &K1);

    //X = X+K1*error;
    LIB_LINALG_MUL_CVECSCALAR(&K1, error, &temp_vec);
    LIB_LINALG_SUM_CVEC(&X, &temp_vec, &X);

    //P = (eye(3,3) - K1*H)*P;
    LIB_LINALG_MUL_CVECRVEC(&K1, &H, &temp_matrix);
    LIB_LINALG_SETIDENTITY_RMAT(&eye3);
    LIB_LINALG_DIF_MAT(&eye3, &temp_matrix, &temp_matrix2);
    LIB_LINALG_MUL_RMATRMAT_SET(&temp_matrix2, &P, &P);


/*
    X = A*X + B*I(k);                   %state predication
    P = A*P*A' + Q_noise;               %covariance prediction

    dOCV_value = interp1(SOC_lookupR, dOCV, SOC, 'linear', 'extrap');
    H = [dOCV_value -1 -1];

    error = voltage(k) - Ut;     %error between prediction and actual

    K1 = P*H'*(H*P*H' + R_noise)^-1;    %kalman gain calc

    X = X+K1*error;          %state matrix update

    P = (eye(3,3) - K1*H)*P; %covariance matrix update
*/

    //Vs(k) = Ut;       optional, track simulated pack voltage
    //SOC_k(k) = X(1);  pack SOC update
    
    Cell_param.SOC = (X).elemCol[1]*100;
    //*packAH= Total_packAH * Cell_param.SOC/100.0f;
}
