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
    HW_GPIO_5V_NFLT2, /*was HW_GPIO_5V_~FLT2~*/ 
    HW_GPIO_5V_NEN1, /*was HW_GPIO_5V_~EN1~*/ 
    HW_GPIO_VCU3_EN,
    HW_GPIO_MC_EN,
    HW_GPIO_LP5_LATCH,
    /*----ADC/DAC STARTS HERE----*/
    HW_GPIO_ADC_MUX_LP2_SNS, /*Added MUX*/
    HW_GPIO_ADC_MUX_LP3_SNS, /*Added MUX*/
    HW_GPIO_ADC_MUX_LP4_SNS, /*Added MUX*/
    HW_GPIO_ADC_MUX_LP5_SNS, /*Added MUX*/
    HW_GPIO_ADC_UVL_BATT,
    HW_GPIO_ADC_MUX2_THERMISTORS, //was Thermist''E''rs /*Added MUX2*/ 
    HW_GPIO_ADC_MUX2_HP_CS, /*was HW_GPIO_HP_SNS*/ /*Added MUX2*/
    HW_GPIO_ADC_5V_VOLTAGE,
    HW_GPIO_DAC_BRAKE_IN, /*THE ONLY DAC*/
    HW_GPIO_ADC_MUX_LP1_SNS, /*Added MUX*/
    HW_GPIO_ADC_MUX_LP6_SNS, /*Added MUX*/
    HW_GPIO_ADC_MUX_LP7_SNS, /*Added MUX*/
    HW_GPIO_ADC_MUX_LP8_SNS, /*Added MUX*/
    HW_GPIO_ADC_MUX_LP9_SNS, /*Added MUX*/
    /*----ADC/DAC ENDS HERE----*/
    HW_GPIO_MUX2_SEL1, /*Added MUX2, was MUX*/
    HW_GPIO_MUX2_SEL2, /*Added MUX2, was MUX*/
    HW_GPIO_PUMP_EN,
    HW_GPIO_PUMP_FAULT,
    HW_GPIO_FAN_EN, /* was HW_GPIO_FAN~~HP~~_EN */
    HW_GPIO_FAN_FAULT, /* was HW_GPIO_FAN~~HP~~_EN */ 
    HW_GPIO_SPARE_EN,
    HW_GPIO_BMS4_EN,
    HW_GPIO_OL_DETECT,
    HW_GPIO_VCU_SFTY_RESET, /*was HW_GPIO_RESET_5V*/ 
    HW_GPIO_VCU_SFTY_EN,
    HW_GPIO_CAN2_RX,
    HW_GPIO_CAN2_TX, /*removed 2 from ..._2TX*/
    HW_GPIO_VC1_EN,
    HW_GPIO_VC2_EN,
    HW_GPIO_LP4_LATCH,
    HW_GPIO_HP_SNS_EN, /*was IO_S_EN*/ //shem says IO_S_EN, but HW_GPIO_SNS_EN is used in code
    HW_GPIO_DIA_EN,
    HW_GPIO_BSPD_MEM,
    HW_GPIO_SPI_NCS_SD, // was IO_CS_SD
    HW_GPIO_SHUTDOWN_EN,
    HW_GPIO_MUX_LP_SEL1, /*Added MUX_LP*/ 
    HW_GPIO_MUX_LP_SEL2, /*Added MUX_LP*/ 
    HW_GPIO_PWM1,
    HW_GPIO_PWM2,
    HW_GPIO_VCU1_EN,
    HW_GPIO_VCU2_EN,
    HW_GPIO_LP8_LATCH,
    HW_GPIO_UART_EN,
    HW_GPIO_CAN1_RX,
    HW_GPIO_CAN1_TX,
    HW_GPIO_SPI_NCS_IMU, /*SPI bus*/ /*was CS_IMD(typo)*/
    HW_GPIO_UART_TX_MCU, /* Remains .._TX_.. */
    HW_GPIO_UART_RX_MCU, /* Changed to .._RX_.*/
    HW_GPIO_BMS3_EN,
    HW_GPIO_5V_NFLT1, /*was HW_GPIO_5V_~FLT1~*/ 
    HW_GPIO_HVE_EN,
    HW_GPIO_COCKPIT_EN,
    HW_GPIO_LP6_LATCH,
    HW_GPIO_BMS5_EN,
    HW_GPIO_BMS6_EN,
    HW_GPIO_LP9_LATCH,
    HW_GPIO_LP7_LATCH, 
    HW_GPIO_SPI_SCK_MCU, /*SPI bus*/ 
    HW_GPIO_SPI_MISO_MCU, /*SPI bus*/ 
    HW_GPIO_SPI_MOSI_MCU, /*SPI bus*/ 
    HW_GPIO_SENSOR_EN,
    HW_GPIO_LP3_LATCH,
    HW_GPIO_LED,
    HW_GPIO_LP1_LATCH,
    HW_GPIO_5V_NEN2,/*was HW_GPIO_5V_~EN2~*/ 
    HW_GPIO_BMS1_EN,
    HW_GPIO_COUNT // LEAVE THIS IN??? NOT IN SCHEM OR EXCEL
} HW_GPIO_pinmux_E;
