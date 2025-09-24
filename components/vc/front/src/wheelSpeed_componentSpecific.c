/**
 * @file wheelSpeed_componentSpecific.c
 * @brief  Source file for component specific wheel speed sensors
 */

/******************************************************************************
 *                             I N C L U D E S
******************************************************************************/

#include "wheelSpeed.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const wheelSpeed_config_E wheelSpeed_config = {
    .sensorType = {
        [WHEEL_FL] = WS_SENSORTYPE_TIM_CHANNEL,
        [WHEEL_FR] = WS_SENSORTYPE_TIM_CHANNEL,
        [WHEEL_RL] =  WS_SENSORTYPE_CAN_RPM,
        [WHEEL_RR] =  WS_SENSORTYPE_CAN_RPM,
    },
    .config = {
        [WHEEL_FL].channel_freq = HW_TIM_CHANNEL_WS_L,
        [WHEEL_FR].channel_freq = HW_TIM_CHANNEL_WS_R,
        [WHEEL_RL].rpm = CANRX_get_signal_func(VEH, VCREAR_wheelSpeedRotationalL),
        [WHEEL_RR].rpm = CANRX_get_signal_func(VEH, VCREAR_wheelSpeedRotationalR),
    },
};
