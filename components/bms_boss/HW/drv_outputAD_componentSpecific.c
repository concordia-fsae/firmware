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
    [DRV_OUTPUTAD_DIGITAL_STATUS_BMS] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_BMS_STATUS,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
#if BMSB_CONFIG_ID == 0U
    [DRV_OUTPUTAD_DIGITAL_STATUS_IMD] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_IMD_STATUS,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
#endif
    [DRV_OUTPUTAD_DIGITAL_AIR] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_AIR,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_DIGITAL_PRECHG] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_PCHG,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_DIGITAL_LED] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_LED,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
};
