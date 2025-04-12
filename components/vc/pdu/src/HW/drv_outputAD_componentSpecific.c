/**
 * @file drv_outputAD_componentSpecific.c
 * @brief Source file for the component specific output driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_outputAD.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

drv_outputAD_configDigital_S drv_outputAD_configDigital[DRV_OUTPUTAD_DIGITAL_COUNT] = {
    [DRV_OUTPUTAD_DIGITAL_LED] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_LED,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
};
