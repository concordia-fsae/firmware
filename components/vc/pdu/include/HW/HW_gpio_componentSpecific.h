/**
 * @file HW_gpio_componentSpecific.h
 * @brief  Header file for GPIO firmware component specific
 */

#pragma once

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/


typedef enum 
{
    HW_GPIO_BMS2_EN = 0U,
    HW_GPIO_ACCUM_EN,
    HW_GPIO_LP2_LATCH,
    HW_GPIO_BSPD_MEM,
    HW_GPIO_SPI_CS_SD, /*CS ELEMENT*/
    HW_GPIO_VCU3_EN,
    HW_GPIO_MC_EN,
    HW_GPIO_LP5_LATCH,
    /*----ADC/DAC STARTS HERE----*/
    HW_GPIO_ADC_LP2_SNS, 
    HW_GPIO_ADC_LP3_SNS,
    HW_GPIO_ADC_LP4_SNS,
    HW_GPIO_ADC_LP5_SNS,
    HW_GPIO_ADC_UVL_BATT,
    HW_GPIO_ADC_THERMISTERS,
    HW_GPIO_ADC_HP_SPI_CS, /*CS ELEMENT*/
    HW_GPIO_ADC_5V_VOLTAGE,
    HW_GPIO_DAC_BRAKE_IN, /*THE ONLY DAC*/
    HW_GPIO_ADC_5V_SNS,
    HW_GPIO_ADC_LP1_SNS,
    HW_GPIO_ADC_LP6_SNS,
    HW_GPIO_ADC_LP7_SNS,
    HW_GPIO_ADC_LP8_SNS,
    HW_GPIO_ADC_LP9_SNS,
    /*----ADC/DAC ENDS HERE----*/
    HW_GPIO_BMS_STATUS,
    HW_GPIO_MUX_SEL1,
    HW_GPIO_MUX_SEL2,
    HW_GPIO_PUMP_EN,
    HW_GPIO_PUMP_FAULT,
    HW_GPIO_FANHP_EN,
    HW_GPIO_FANHP_FAULT,
    HW_GPIO_SPARE_EN,
    HW_GPIO_BMS4_EN,
    HW_GPIO_OL_DETECT,
    HW_GPIO_RESET_5V,
    HW_GPIO_VCU_SFTY_EN,
    HW_GPIO_CAN2_RX,
    HW_GPIO_CAN2_TX, /*removed 2 from ..._2TX*/
    HW_GPIO_VC1_EN,
    HW_GPIO_VC2_EN,
    HW_GPIO_LP4_LATCH,
    HW_GPIO_HP_SPI_CS_EN, /*ADDED ..._SPI_CS_...*/ /*CS ELEMENT*/
    HW_GPIO_DIA_EN,
    HW_GPIO_5V_FLT2,
    HW_GPIO_5V_EN1,
    HW_GPIO_5V_EN2,
    HW_GPIO_5V_FLT1,
    HW_GPIO_MUX_LP_SEL2, /*ADDED ..._MUX_LP_...*/
    HW_GPIO_PWM1,
    HW_GPIO_PWM2,
    HW_GPIO_VCU1_EN,
    HW_GPIO_VCU2_EN,
    HW_GPIO_LP8_LATCH,
    HW_GPIO_UART_EN,
    HW_GPIO_BMS_MEM,
    HW_GPIO_CAN1_RX,
    HW_GPIO_CAN1_TX,
    HW_GPIO_SPI_CS_IMD, /*CS ELEMENT*/
    HW_GPIO_UART_TX_MCU_PP, /* DUPLICATE? TYPE:PP*/
    HW_GPIO_UART_TX_MCU_IN, /* DUPLICATE? TYPE:INPUT*/
    HW_GPIO_BMS3_EN,
    HW_GPIO_MUX_LP_SEL1, /*ADDED ..._MUX_LP_...*/
    HW_GPIO_HVE_EN,
    HW_GPIO_COCKPIT_EN,
    HW_GPIO_LP6_LATCH,
    HW_GPIO_BMS5_EN,
    HW_GPIO_BMS6_EN,
    HW_GPIO_LP9_LATCH,
    HW_GPIO_LP7_LATCH,
    HW_GPIO_SCK_MCU,
    HW_GPIO_MISO_MCU,
    HW_GPIO_MOSI_MCU,
    HW_GPIO_SENSOR_EN,
    HW_GPIO_LP3_LATCH,
    HW_GPIO_LED,
    HW_GPIO_LP1_LATCH,
    HW_GPIO_SHUTDOWN_EN,
    HW_GPIO_BMS1_EN,
    HW_GPIO_COUNT
} HW_GPIO_pinmux_E;
