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
    [DRV_OUTPUTAD_BMS1_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_BMS1_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_BMS2_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_BMS2_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_BMS3_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_BMS3_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_BMS4_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_BMS4_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_BMS5_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_BMS5_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_BMS6_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_BMS6_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_LP1_LATCH] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_LP1_LATCH, 
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_LP2_LATCH] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_LP2_LATCH, 
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_LP3_LATCH] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_LP3_LATCH, 
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_LP4_LATCH] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_LP4_LATCH, 
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_LP5_LATCH] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_LP5_LATCH, 
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_LP6_LATCH] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_LP6_LATCH, 
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_LP7_LATCH] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_LP7_LATCH, 
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_LP8_LATCH] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_LP8_LATCH, 
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_LP9_LATCH] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_LP9_LATCH, 
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_PUMP_FAULT] = { 
    .type = OUTPUT_DIGITAL,
    .config.gpio = {
        .pin = HW_GPIO_PUMP_FAULT, 
        .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_FAN_FAULT] = { 
    .type = OUTPUT_DIGITAL,
    .config.gpio = {
        .pin = HW_GPIO_FAN_FAULT, 
        .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_ACCUM_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_ACCUM_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_5V_NEN1] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_5V_NEN1, 
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_5V_NEN2] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_5V_NEN2, 
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
    [DRV_OUTPUTAD_VCU1_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_VCU1_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_VCU2_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_VCU2_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_VCU3_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_VCU3_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_MC_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_MC_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_MUX2_SEL1] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_MUX2_SEL1, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_MUX2_SEL2] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_MUX2_SEL2, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_PUMP_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_PUMP_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_FAN_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_FAN_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_SPARE_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_SPARE_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_VCU_SFTY_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_VCU_SFTY_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_VC1_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_VC1_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_VC2_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_VC2_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_HP_SNS_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_HP_SNS_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_DIA_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_DIA_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_SHUTDOWN_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_SHUTDOWN_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_MUX_LP_SEL1] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_MUX_LP_SEL1, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_MUX_LP_SEL2] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_MUX_LP_SEL2, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
#if FEATURE_IS_ENABLED(FEATURE_PUMP_FULL_BEANS)
    [DRV_OUTPUTAD_PWM1] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_PWM1, 
            .active_level = DRV_IO_LOGIC_LOW,
        },
    },
#endif
    [DRV_OUTPUTAD_PWM2] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_PWM2, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_UART_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_UART_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_HVE_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_HVE_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_COCKPIT_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_COCKPIT_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_SENSOR_EN] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_SENSOR_EN, 
            .active_level = DRV_IO_LOGIC_HIGH,
        },
    },
    [DRV_OUTPUTAD_OL_DETECT] = { 
        .type = OUTPUT_DIGITAL,
        .config.gpio = {
            .pin = HW_GPIO_OL_DETECT, 
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
