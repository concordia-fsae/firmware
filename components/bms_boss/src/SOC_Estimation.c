/**
 * @file SOC_Estimation.c
 * @brief  Source code for SOC Estimation
*/

#include <string.h>

/**< Module headers */
#include "SOC_Estimation.h"
#include "BMS.h"
/**< Driver Includes */

/**< Other Includes */
#include "SOC_Interpolation_Maps.h"
#include "lib_interpolation.h"
#include "lib_linAlg.h"

#define CELL_AH 4.5f
#define R_noise 0.05f

//SOC lookup
const float32_t SOC_start = 100; //this must come from nvm in soc_init function

LIB_LINALG_DEFINE_N(socMatrix, float32_t, 3U);
typedef LIB_LINALG_INST_RMAT(socMatrix) soc_matrix_S;

LIB_LINALG_DEFINE_N(socVector, float32_t, 3U);
typedef LIB_LINALG_INST_CVEC(socVector) soc_col_vector_S;
typedef LIB_LINALG_INST_ROW(socVector) soc_row_vector_S;

static soc_matrix_S Q_noise =  { {{7e-8, 0, 0}, {0, 6e-5, 0}, {0, 0, 6e-5}} }; //Process noise
static soc_matrix_S P = { {{1e-4, 0, 0}, {0, 1e-4, 0}, {0, 0, 1e-4}} }; //Covariance

static soc_col_vector_S X =  {0}; //state //soc_start is a placeholder for nvm soc at startup

soc_col_vector_S tmp_vec = {0};
soc_col_vector_S tmp_vec2 = {0};
soc_matrix_S eye3 = {0};
soc_matrix_S tmp_matrix = {0};
soc_matrix_S tmp_matrix2 = {0};
//^ all math logic is in 0-1 soc not 0-100%

batteryModel_S batteryModel;

/******************************************************************************
 *                             Private Functions
 ******************************************************************************/

static void cell_params_update(float32_t SOC){
    Cell_param.SOC = SOC;
    Cell_param.OCV = (lib_interpolation_interpolate(&SOC_OCV_Func, Cell_param.SOC*100));
    Cell_param.Ri = (lib_interpolation_interpolate(&SOC_Ri_FUNC, Cell_param.SOC*100));
    Cell_param.R1 = (lib_interpolation_interpolate(&SOC_R1_FUNC, Cell_param.SOC*100));
    Cell_param.C1 = (lib_interpolation_interpolate(&SOC_C1_FUNC, Cell_param.SOC*100));
    Cell_param.R2 = (lib_interpolation_interpolate(&SOC_R2_FUNC, Cell_param.SOC*100));
    Cell_param.C2 = (lib_interpolation_interpolate(&SOC_C2_FUNC, Cell_param.SOC*100));
}

static void model_state_run(float32_t cell_voltage, float32_t cell_current, float64_t dt){

    float32_t tau1 = Cell_param.R1*Cell_param.C1; 
    float32_t tau2 = Cell_param.R2*Cell_param.C2;

    soc_matrix_S A = { {{1, 0, 0}, {0, exp(-dt/tau1), 0}, {0, 0, exp(-dt/tau2)}} };
    soc_col_vector_S B = {-dt/(CELL_AH*3600), Cell_param.R1*(1-exp(-dt/tau1)), Cell_param.R2*(1-exp(-dt/tau2))};

    //X = A*X + B*I(k); 
    LIB_LINALG_MUL_RMATCVEC_SET(&A, &X, &tmp_vec);
    LIB_LINALG_MUL_CVECSCALAR(&B, cell_current, &tmp_vec2);
    LIB_LINALG_SUM_CVEC(&tmp_vec, &tmp_vec2, &X);

    //P = A*P*A' + Q_noise;
    LIB_LINALG_TRANSPOSE_MAT_GET(&A, &tmp_matrix);
    LIB_LINALG_MUL_RMATRMAT_SET(&P, &tmp_matrix, &tmp_matrix2);
    LIB_LINALG_MUL_RMATRMAT_SET(&A, &tmp_matrix2, &tmp_matrix);
    LIB_LINALG_SUM_MAT(&tmp_matrix, &Q_noise, &P);

    float32_t dOCV = lib_interpolation_interpolate(&SOC_dOCV_Func, X.elemCol[0]*100);
    soc_row_vector_S H = {dOCV, -1, -1}; //jacobian vector

    Circuit_param.voltage = Cell_param.OCV - X.elemCol[1] - X.elemCol[2] - (cell_current)*Cell_param.Ri;
    float32_t error = cell_voltage - Circuit_param.voltage;
    float32_t scalar = 0;

    //K = P*H'*(H*P*H' + R_noise)^-1;
    LIB_LINALG_TRANSPOSE_RVEC_GET(&H, &tmp_vec);
    LIB_LINALG_MUL_RMATCVEC_SET(&P, &tmp_vec, &tmp_vec2);
    LIB_LINALG_MUL_RVECCVEC_SET(&H, &tmp_vec2, &scalar);
    scalar = 1/(scalar + R_noise);
    soc_col_vector_S K =  {0}; //Kalman Gain
    LIB_LINALG_TRANSPOSE_RVEC_GET(&H, &tmp_vec);
    LIB_LINALG_MUL_RMATCVEC_SET(&P, &tmp_vec, &tmp_vec2);
    LIB_LINALG_MUL_CVECSCALAR(&tmp_vec2, scalar, &K);

    //X = X+K1*error;
    LIB_LINALG_MUL_CVECSCALAR(&K, error, &tmp_vec);
    LIB_LINALG_SUM_CVEC(&X, &tmp_vec, &X);

    //P = (eye(3,3) - K1*H)*P;
    LIB_LINALG_MUL_CVECRVEC(&K, &H, &tmp_matrix);
    LIB_LINALG_SETIDENTITY_RMAT(&eye3);
    LIB_LINALG_DIF_MAT(&eye3, &tmp_matrix, &tmp_matrix2);
    LIB_LINALG_MUL_RMATRMAT_SET(&tmp_matrix2, &P, &tmp_matrix);
    P=tmp_matrix;

    if (X.elemCol[0] > 1.0f) X.elemCol[0] = 1.0f;
    if (X.elemCol[0] < 0.0f) X.elemCol[0] = 0.0f;

    // update cell/circuit parameters
    cell_params_update(X.elemCol[0]);
    Circuit_param.VRC1 = X.elemCol[1];
    Circuit_param.VRC2 = X.elemCol[2];
}

static void current_limit(float32_t cell_voltage, float32_t cell_current){

    if (cell_current < 0 || cell_voltage < Cell_param.OCV){
    batteryModel.max_current = (Cell_param.OCV-Circuit_param.VRC1-Circuit_param.VRC2-2.5)/Cell_param.Ri;
    } else if (cell_current > 0 || cell_voltage > Cell_param.OCV){
    batteryModel.max_current = -(Cell_param.OCV-Circuit_param.VRC1-Circuit_param.VRC2-4.1)/Cell_param.Ri;
    }
}

/******************************************************************************
 *                             Public Functions
 ******************************************************************************/

 void SOC_Estimation_init(void){
    batteryModel.state = INIT;
    cell_params_update(current_data.soc); ///get from nvm in init_SOC()
    
    Circuit_param.VRC1 = 0;
    Circuit_param.current = 0;
    Circuit_param.voltage = 0;

    batteryModel.last_step = 0;
    batteryModel.VRC2_estimate = 0;
 }

 //check input parameters
void SOC_Estimation(float32_t pack_voltage, float32_t pack_current, float64_t time_now){

    float32_t cell_voltage = pack_voltage/BMS_CONFIGURED_SERIES_CELLS;
    float32_t cell_current = pack_current/BMS_CONFIGURED_PARALLEL_CELLS;
    float64_t dt = (time_now - batteryModel.last_step)/1000000.0;

    float32_t tau1 = Cell_param.R1*Cell_param.C1; 
    float32_t tau2 = Cell_param.R2*Cell_param.C2;

    if (batteryModel.state == INIT){
        batteryModel.VRC2_estimate = cell_voltage;
        batteryModel.last_step = time_now;
        batteryModel.state = VRC_ESTIMATE;
    }
    else if (batteryModel.state == VRC_ESTIMATE && (time_now - batteryModel.last_step)/1000000 >= 5){ //wait 5s to find VRC2

        float32_t dV = cell_voltage - batteryModel.VRC2_estimate;
        float32_t VRC2_old = dV/(1-exp(-dt/tau2));
        batteryModel.VRC2_estimate = VRC2_old*exp(-dt/tau2);
        Circuit_param.VRC2 = batteryModel.VRC2_estimate;

        X.elemCol[0] = Cell_param.SOC;
        X.elemCol[1] = Circuit_param.VRC1;
        X.elemCol[2] = Circuit_param.VRC2;
        batteryModel.state = RUNNING;
    }
    else if (batteryModel.state == RUNNING) {

        model_state_run(cell_voltage, cell_current, dt);
        current_limit(cell_voltage, cell_current);
    }
}
