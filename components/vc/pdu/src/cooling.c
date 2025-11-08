/**
 * @file cooling.c
 * @brief  Source code for Cooling Application
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "app_vehicleState.h"
#include "drv_outputAD.h"
#include "drv_vn9008.h"
#include "HW_tim.h"
#include "Module.h"

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
    const drv_hsd_state_E pump_state = drv_vn9008_getState(DRV_VN9008_CHANNEL_PUMP);
    const drv_hsd_state_E fan_state  = drv_vn9008_getState(DRV_VN9008_CHANNEL_FAN);

    switch (pump_state)
    {
        case DRV_HSD_STATE_OFF:
            if ((app_vehicleState_getState() == VEHICLESTATE_ON_HV) || (app_vehicleState_getState() == VEHICLESTATE_TS_RUN))
            {
                drv_vn9008_setEnabled(DRV_VN9008_CHANNEL_PUMP, true);    // Power of pump controlled by the cooling manager
#if FEATURE_IS_DISABLED(FEATURE_PUMP_FULL_BEANS)
                HW_TIM_setDuty(HW_TIM_PORT_PUMP, HW_TIM_CHANNEL_1, 0.75f);
#endif
            }
            else
            {
#if FEATURE_IS_DISABLED(FEATURE_PUMP_FULL_BEANS)
                HW_TIM_setDuty(HW_TIM_PORT_PUMP, HW_TIM_CHANNEL_1, 0.00f);
#endif
            }
            break;

        case DRV_HSD_STATE_ON:
            if (app_vehicleState_getState() != VEHICLESTATE_ON_HV)
            {
                drv_vn9008_setEnabled(DRV_VN9008_CHANNEL_PUMP, false);    // Power of pump controlled by the cooling manager
                if (app_vehicleState_getState() != VEHICLESTATE_TS_RUN)
                {
#if FEATURE_IS_DISABLED(FEATURE_PUMP_FULL_BEANS)
                    HW_TIM_setDuty(HW_TIM_PORT_PUMP, HW_TIM_CHANNEL_1, 0.00f);
#endif
                }
            }
            break;

        case DRV_HSD_STATE_OVERCURRENT:
            if ((app_vehicleState_getState() == VEHICLESTATE_TS_RUN) || (app_vehicleState_getState() == VEHICLESTATE_ON_HV))
            {
                drv_vn9008_setEnabled(DRV_VN9008_CHANNEL_PUMP, false);
            }
            else
            {
#if FEATURE_IS_DISABLED(FEATURE_PUMP_FULL_BEANS)
                HW_TIM_setDuty(HW_TIM_PORT_PUMP, HW_TIM_CHANNEL_1, 0.00f);
#endif
            }
            break;

        case DRV_HSD_STATE_OVERTEMP:
            if (app_vehicleState_getState() != VEHICLESTATE_TS_RUN)
            {
                drv_vn9008_setEnabled(DRV_VN9008_CHANNEL_PUMP, false);
            }
            if (app_vehicleState_getState() != VEHICLESTATE_ON_HV)
            {
#if FEATURE_IS_DISABLED(FEATURE_PUMP_FULL_BEANS)
                HW_TIM_setDuty(HW_TIM_PORT_PUMP, HW_TIM_CHANNEL_1, 0.00f);
#endif
            }
            break;

        default:
            if ((app_vehicleState_getState() == VEHICLESTATE_TS_RUN) || (app_vehicleState_getState() == VEHICLESTATE_ON_HV))
            {
                drv_vn9008_setEnabled(DRV_VN9008_CHANNEL_PUMP, false);
            }
#if FEATURE_IS_DISABLED(FEATURE_PUMP_FULL_BEANS)
            HW_TIM_setDuty(HW_TIM_PORT_PUMP, HW_TIM_CHANNEL_1, 0.00f);
#endif
            break;
    }
    else if (app_vehicleState_getState() == VEHICLESTATE_TS_RUN)
    {
        drv_vn9008_setEnabled(DRV_VN9008_CHANNEL_FAN, true);
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
