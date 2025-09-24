/**
 * @file wheelSpeed_componentSpecific.c
 * @brief  Source file for component specific wheel speed sensors
 */

/******************************************************************************
 *                             I N C L U D E S
******************************************************************************/

#include "wheelSpeed.h"
#include "MessageUnpack_generated.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const wheelSpeed_config_E wheelSpeed_config = {
    .sensorType = {
        [WHEEL_FL] = WS_SENSORTYPE_CAN_RPM,
        [WHEEL_FR] = WS_SENSORTYPE_CAN_RPM,
        [WHEEL_RL] = WS_SENSORTYPE_TIM_CHANNEL,
        [WHEEL_RR] = WS_SENSORTYPE_TIM_CHANNEL,
    },
    .config = {
        [WHEEL_FL].rpm = CANRX_get_signal_func(VEH, VCFRONT_wheelSpeedRotationalFL),
        [WHEEL_FR].rpm = CANRX_get_signal_func(VEH, VCFRONT_wheelSpeedRotationalFR),
        [WHEEL_RL].channel_freq = HW_TIM_CHANNEL_WS_L,
        [WHEEL_RR].channel_freq = HW_TIM_CHANNEL_WS_R,
    },
};
