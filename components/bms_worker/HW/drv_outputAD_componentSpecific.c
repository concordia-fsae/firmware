/**
 * @file drv_outputAD_componentSpecific.c
 * @brief Source file for the component specific output driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "include/drv_outputAD_componentSpecific.h"
#include "drv_outputAD.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

drv_outputAD_configDigital_S drv_outputAD_configDigital[DRV_OUTPUTAD_DIGITAL_COUNT] = {
    [DRV_OUTPUTAD_DIGITAL_MUX_SEL1] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_MUX_SEL1,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_DIGITAL_MUX_SEL2] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_MUX_SEL2,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_DIGITAL_MUX_SEL3] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_MUX_SEL3,
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
#if APP_VARIANT_ID == 1
    [DRV_OUTPUTAD_DIGITAL_SUPPLY_KEEPON] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_SUPPLY_KEEPON,
            .active_level = DRV_IO_LOGIC_HIGH
        },
    }
#endif
};
