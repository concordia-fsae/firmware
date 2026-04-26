/**
 * @file wheelSpeed_componentSpecific.c
 * @brief  Source file for component specific wheel speed sensors
 */

/******************************************************************************
 *                             I N C L U D E S
******************************************************************************/

#include "HW_tim_componentSpecific.h"
#include "app_vehicleSpeed.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const app_wheelSpeed_config_S app_wheelSpeed_config = {
    .sensorType = {
        [WHEEL_FL] = WS_SENSORTYPE_UNKNOWN,
        [WHEEL_FR] = WS_SENSORTYPE_UNKNOWN,
        [WHEEL_RL] = WS_SENSORTYPE_UNKNOWN,
        [WHEEL_RR] = WS_SENSORTYPE_UNKNOWN,
    },
};
