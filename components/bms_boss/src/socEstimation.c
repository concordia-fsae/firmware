/**< Module header */
#include "socEstimation.h"

/**< Driver Includes */
#include "HW.h"
#include "HW_adc.h"
#include "HW_tim.h"

/**< Other Includes */
#include "Module.h"
#include "string.h"
#include <stdint.h>

#include "MessageUnpack_generated.h"
#include "FeatureDefines_generated.h"
#include "lib_interpolation.h"
#include "drv_timer.h"
#include "lib_nvm.h"

#define Total_packAH 18.0f
#define Max_voltage 4.20f
#define Min_OCV 2.50f
#define Min_voltage 2.20f
#define Resistance_offset 0.0f

static struct
{
    float32_t voltage;
    float32_t OCV;
    float32_t SOC;
    float32_t R0;
    float32_t R1;
    float32_t C1;
} Cell_param;

static struct 
{
    float32_t VRC1; // voltage drop over RC network 1
    float32_t dVRC1; //change in voltage drop over RC network 1
    float32_t VR1; //voltage drop over resistor 1
    float32_t current; // current draw as RX from boss note: current is negative for discharge
    float32_t voltage;
} Circuit_param;

/******************************************************************************
 *                             Interpolations Maps
 ******************************************************************************/

static lib_interpolation_point_S OCV_SOCMap[] = {
    {
        .x = 2.5f, // OCV
        .y = 0.0f, //SOC
    },
    {
        .x = 3.178f, 
        .y = 10.0f, 
    },
    {
        .x = 3.37f, 
        .y = 20.0f, 
    },
    {
        .x = 3.52f, 
        .y = 30.0f, 
    },
    {
        .x = 3.62f, 
        .y = 40.0f, 
    },
    {
        .x = 3.75f, 
        .y = 50.0f, 
    },
    {
        .x = 3.84f, 
        .y = 60.0f, 
    },
    {
        .x = 3.94f, 
        .y = 70.0f, 
    },
    {
        .x = 4.05f, 
        .y = 80.0f, 
    },
    {
        .x = 4.09f, 
        .y = 90.0f, 
    },
    {
        .x = 4.20f, 
        .y = 100.0f, 
    },
};

static lib_interpolation_mapping_S OCV_SOC_Func = {
    .points = (lib_interpolation_point_S*)&OCV_SOCMap,
    .number_points = COUNTOF(OCV_SOCMap),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_point_S SOC_R0Map[] = {
    {
        .x = 10.0f, //SOC
        .y = 0.02f + Resistance_offset, //R0
    },
    {
        .x = 20.0f, 
        .y = 0.0183f + Resistance_offset, 
    },
    {
        .x = 30.0f, 
        .y = 0.0170f + Resistance_offset, 
    },
    {
        .x = 40.0f, 
        .y = 0.016757f + Resistance_offset, 
    },
    {
        .x = 50.0f, 
        .y = 0.016757f + Resistance_offset, 
    },
    {
        .x = 60.0f, 
        .y = 0.014595f+ Resistance_offset, 
    },
    {
        .x = 70.0f, 
        .y = 0.0151f + Resistance_offset, 
    },
    {
        .x = 80.0f, 
        .y = 0.017568f + Resistance_offset, 
    },
    {
        .x = 90.0f, 
        .y = 0.0181f + Resistance_offset, 
    },
    {
        .x = 100.0f, 
        .y = 0.0213f + Resistance_offset, 
    },
};

static lib_interpolation_mapping_S SOC_R0_FUNC = {
    .points = (lib_interpolation_point_S*)&SOC_R0Map,
    .number_points = COUNTOF(SOC_R0Map),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_point_S SOC_R1Map[] = {
    {
        .x = 10.0f, //SOC
        .y = 0.0049f, //R1
    },
    {
        .x = 20.0f, 
        .y = 0.0032f, 
    },
    {
        .x = 30.0f, 
        .y = 0.002f, 
    },
    {
        .x = 40.0f, 
        .y = 0.0019f, 
    },
    {
        .x = 50.0f, 
        .y = 0.0019f, 
    },
    {
        .x = 60.0f, 
        .y = 0.0021f, 
    },
    {
        .x = 70.0f, 
        .y = 0.0024f, 
    },
    {
        .x = 80.0f, 
        .y = 0.0028f, 
    },
    {
        .x = 90.0f, 
        .y = 0.0023f, 
    },
    {
        .x = 100.0f, 
        .y = 0.0045f, 
    },
};

static lib_interpolation_mapping_S SOC_R1_FUNC = {
    .points = (lib_interpolation_point_S*)&SOC_R1Map,
    .number_points = COUNTOF(SOC_R1Map),
    .saturate_left = true,
    .saturate_right = true,
};

static lib_interpolation_point_S SOC_C1Map[] = {
    {
        .x = 10.0f, //SOC
        .y = 697.32f, //C1
    },
    {
        .x = 20.0f, 
        .y = 1748.00f, 
    },
    {
        .x = 30.0f, 
        .y = 1935.5f, 
    },
    {
        .x = 40.0f, 
        .y = 1587.1f, 
    },
    {
        .x = 50.0f, 
        .y = 1456.5f, 
    },
    {
        .x = 60.0f, 
        .y = 447.43f, 
    },
    {
        .x = 70.0f, 
        .y = 671.78f, 
    },
    {
        .x = 80.0f, 
        .y = 1376.00f, 
    },
    {
        .x = 90.0f, 
        .y = 1266.2f, 
    },
    {
        .x = 100.0f, 
        .y = 625.55f, 
    },
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
static void ocv_soc(void){
    Cell_param.SOC = (lib_interpolation_interpolate(&OCV_SOC_Func, Cell_param.OCV));
};

static void SOC_R0(void){  /*this is most important R value so only add offset here*/
    Cell_param.R0 = (lib_interpolation_interpolate(&SOC_R0_FUNC, Cell_param.SOC));
};

static void SOC_R1(void){
    Cell_param.R1 = (lib_interpolation_interpolate(&SOC_R1_FUNC, Cell_param.SOC));
}

static void SOC_C1(void){
    Cell_param.C1 = (lib_interpolation_interpolate(&SOC_C1_FUNC, Cell_param.SOC));
}

static void init_SOC()
{
    Cell_param.SOC = (current_data.pack_amp_hours / Total_packAH) * 100.0f;
}


/******************************************************************************
 *                             Public Functions
 ******************************************************************************/

 void socEstimation_init(void){
    Cell_param.voltage =0;
    Cell_param.R0 = 0;
    Cell_param.R1 = 0;
    Cell_param.C1 = 0;
    Cell_param.OCV = 0;
    Circuit_param.VRC1 = 0;
    Circuit_param.dVRC1 = 0;
    Circuit_param.current = 0;
    Circuit_param.VR1 = 0;
    init_SOC();
 }

void SOCestimation(float32_t *packAH,float32_t *packVoltage,float32_t *packCurrent,const uint32_t *dt_us){
    // get all circuit parameters
    SOC_R0();
    SOC_R1();
    SOC_C1();
    float32_t dt = (float32_t)(*dt_us) * 0.000001f;


    Circuit_param.dVRC1 = ((-Circuit_param.VRC1/(Cell_param.R1*Cell_param.C1)) - (*packCurrent)/Cell_param.C1) * (dt);
    Circuit_param.VRC1 = Circuit_param.VRC1 + Circuit_param.dVRC1;
    Cell_param.OCV = (*packVoltage) + ((*packCurrent)*Cell_param.R1) + Circuit_param.VRC1;
    ocv_soc(); // update soc with new ocv value 
    *packAH= Total_packAH * Cell_param.SOC/100.0f;
}

const ModuleDesc_S socEstimation_desc = {
    .moduleInit        = &socEstimation_init,
};
