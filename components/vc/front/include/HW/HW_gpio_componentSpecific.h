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
     HW_GPIO_DIG_SPARE1 = 0U,
     HW_GPIO_DIG_SPARE2,
     HW_GPIO_DIG_SPARE3,
     HW_GPIO_DIG_SPARE4,
     HW_GPIO_LED,
     HW_GPIO_ADC_SPARE1, //ADC ADDED FOR THE NEXT FEW AND AIN REMOVED
     HW_GPIO_ADC_SPARE2, 
     HW_GPIO_ADC_SPARE3, 
     HW_GPIO_ADC_SPARE4, 
     HW_GPIO_ADC_APPS_P1,
     HW_GPIO_ADC_APPS_P2,
     HW_GPIO_ADC_BR_POT,
     HW_GPIO_ADC_L_SHK_DISP,
     HW_GPIO_ADC_L_BR_TEMP, //WAS L_BR_TMP
     HW_GPIO_ADC_R_SHK_DISP,
     HW_GPIO_ADC_R_BR_TEMP, //WAS R_BR_TEMP
     HW_GPIO_ADC_PU1,
     HW_GPIO_ADC_PU2,
     HW_GPIO_ADC_BR_PR,
     HW_GPIO_ADC_BOARD_TEMP, //WAS TEMP_1
     HW_GPIO_5V_FLT2,
     HW_GPIO_5V_EN2,
     HW_GPIO_5V_EN1,
     HW_GPIO_5V_FLT1,
     HW_GPIO_CAN2_RX, // WAS HW_GPIO_RXD2
     HW_GPIO_CAN2_TX, // WAS HW_GPIO_TXD2
     HW_GPIO_MUX_SEL2, // WAS SEL2
     HW_GPIO_MUX_SEL1, // WAS SEL1
     HW_GPIO_HORN_EN,
     HW_GPIO_BMS_LIGHT_EN,
     HW_GPIO_IMD_LIGHT_EN,
     HW_GPIO_TSSI_G_EN,
     HW_GPIO_TSSI_R_EN,
     HW_GPIO_BR_LIGHT_EN,
     HW_GPIO_CAN1_RX, // WAS HW_GPIO_RXD1
     HW_GPIO_CAN1_TX, //WAS HW_GPIO_TXD1
     HW_GPIO_MCU_UART_EN,
     HW_GPIO_MCU_UART_TX,
     HW_GPIO_MCU_UART_RX,
     HW_GPIO_SPI_CS_IMU, //WAS CS_IMU
     HW_GPIO_SPI_CS_SD, // WAS CS_SD
     HW_GPIO_SPI_SCK,
     HW_GPIO_SPI_MISO,
     HW_GPIO_SPI_MOSI,
     HW_GPIO_TACH_FLOW, //WAS FLOW
     HW_GPIO_TACH_WHEELSPEED_R, // WAS R_WS
     HW_GPIO_TACH_WHEELSPEED_L, //WAS L_WS
     HW_GPIO_RUN_BUTTON, // WAS RUN
     HW_GPIO_COUNT,
 } HW_GPIO_pinmux_E;
