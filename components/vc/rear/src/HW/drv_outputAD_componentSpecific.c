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
    [DRV_OUTPUTAD_DIGITAL_5V_EN1] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_5V_NEN1,
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_DIGITAL_5V_EN2] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_5V_NEN2,
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
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
    [DRV_OUTPUTAD_DIGITAL_HORN_EN] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_HORN_EN,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_DIGITAL_BMS_LIGHT_EN] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_BMS_LIGHT_EN,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_DIGITAL_IMD_LIGHT_EN] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_IMD_LIGHT_EN,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_DIGITAL_TSSI_G_EN] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_TSSI_G_EN,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_DIGITAL_TSSI_R_EN] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_TSSI_R_EN,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_DIGITAL_BR_LIGHT_EN] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_BR_LIGHT_EN,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_DIGITAL_MCU_UART_EN] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_MCU_UART_EN,
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_DIGITAL_CS_IMU] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_SPI_NCS_IMU,
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_DIGITAL_CS_SD] = {
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_SPI_NCS_SD,
            .active_level = DRV_IO_LOGIC_LOW,
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
