/**
 * @file cooling.c
 * @brief  Source code for Cooling Application
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Module.h"
#include "drv_vn9008.h"
#include "drv_outputAD.h"
#include "app_vehicleState.h"
#include "HW_tim.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Cooling Module Init function
 */
static void cooling_init()
{
#if FEATURE_IS_ENABLED(FEATURE_PUMP_FULL_BEANS)
        drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_PWM1, DRV_IO_ACTIVE);
#endif
}

/**
 * @brief  Cooling Module 1Hz periodic function
 */
static void cooling10Hz_PRD(void)
{
    if ((app_vehicleState_getState() == VEHICLESTATE_ON_HV) ||
        (app_vehicleState_getState() == VEHICLESTATE_TS_RUN))
    {
        // TODO: Make less ret
        drv_vn9008_setEnabled(DRV_VN9008_CHANNEL_FAN, true);
        drv_vn9008_setEnabled(DRV_VN9008_CHANNEL_PUMP, true); // Power of pump controlled by the cooling manager
#if FEATURE_IS_DISABLED(FEATURE_PUMP_FULL_BEANS)
        HW_TIM_setDuty(HW_TIM_PORT_PUMP, HW_TIM_CHANNEL_1, 0.75f);
#endif
    }
    else
    {
        drv_vn9008_setEnabled(DRV_VN9008_CHANNEL_FAN, false);
        drv_vn9008_setEnabled(DRV_VN9008_CHANNEL_PUMP, false); // Power of pump controlled by the cooling manager
#if FEATURE_IS_DISABLED(FEATURE_PUMP_FULL_BEANS)
        HW_TIM_setDuty(HW_TIM_PORT_PUMP, HW_TIM_CHANNEL_1, 0.00f);
#endif
    }
}

/**
 * @brief  Cooling Module descriptor
 */
const ModuleDesc_S cooling_desc = {
    .moduleInit       = &cooling_init,
    .periodic10Hz_CLK = &cooling10Hz_PRD,
};
