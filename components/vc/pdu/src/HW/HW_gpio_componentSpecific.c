/**
 * @file HW_gpio_componentSpecific.c
 * @brief  Source code for GPIO firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW_gpio.h"

const HW_GPIO_S HW_GPIO_pinmux[HW_GPIO_COUNT] = {
    [HW_GPIO_BMS2_EN] = {
        .port = GPIOE,
        .pin = GPIO_PIN_2, // Based on PE2
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_ACCUM_EN] = {
        .port = GPIOE,
        .pin = GPIO_PIN_3, // Based on PE3
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_LP2_LATCH] = {
        .port = GPIOE,
        .pin = GPIO_PIN_4, // Based on PE4
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_5V_NFLT2] = {
        .port = GPIOE,
        .pin = GPIO_PIN_5, // Based on PD11
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //NOSET (inverse of NEN2)
    },
    [HW_GPIO_5V_NEN1] = {
        .port = GPIOE,
        .pin = GPIO_PIN_6, // Based on PE6
        .mode = GPIO_MODE_OUTPUT_OD, //OD 
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_VCU3_EN] = {
        .port = GPIOC,
        .pin = GPIO_PIN_13, // Based on PC13
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_MC_EN] = {
        .port = GPIOC,
        .pin = GPIO_PIN_14, // Based on PC14
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_LP5_LATCH] = {
        .port = GPIOC,
        .pin = GPIO_PIN_15, // Based on PC15
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_ADC_MUX_LP2_SNS] = {
        .port = GPIOC,
        .pin = GPIO_PIN_0, // Based on PC0
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //ANALOG = NOSET
    },
    [HW_GPIO_ADC_MUX_LP3_SNS] = {
        .port = GPIOC,
        .pin = GPIO_PIN_1, // Based on PC1
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //ANALOG = NOSET
    },
    [HW_GPIO_ADC_MUX_LP4_SNS] = {
        .port = GPIOC,
        .pin = GPIO_PIN_2, // Based on PC2
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //ANALOG = NOSET
    },
    [HW_GPIO_ADC_MUX_LP5_SNS] = {
        .port = GPIOC,
        .pin = GPIO_PIN_3, // Based on PC3
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //ANALOG = NOSET
    },
    [HW_GPIO_ADC_UVL_BATT] = {
        .port = GPIOA,
        .pin = GPIO_PIN_1, // Based on PA1
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //ANALOG = NOSET
    },
    [HW_GPIO_ADC_MUX2_THERMISTORS] = {
        .port = GPIOA,
        .pin = GPIO_PIN_2, // Based on PA2
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //ANALOG = NOSET
    },
    [HW_GPIO_ADC_MUX2_HP_CS] = {
        .port = GPIOA,
        .pin = GPIO_PIN_3, // Based on PA3
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //ANALOG = NOSET
    },
    [HW_GPIO_ADC_5V_VOLTAGE] = {
        .port = GPIOA,
        .pin = GPIO_PIN_4, // Based on PA4
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //ANALOG = NOSET
    },
    [HW_GPIO_DAC_BRAKE_IN] = {
        .port = GPIOA,
        .pin = GPIO_PIN_5, // Based on PA5
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //ANALOG = NOSET
    },
    [HW_GPIO_ADC_MUX_LP1_SNS] = {
        .port = GPIOA,
        .pin = GPIO_PIN_7, // Based on PA7
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //ANALOG = NOSET
    },
    [HW_GPIO_ADC_MUX_LP6_SNS] = {
        .port = GPIOC,
        .pin = GPIO_PIN_4, // Based on PC4
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //ANALOG = NOSET
    },
    [HW_GPIO_ADC_MUX_LP7_SNS] = {
        .port = GPIOC,
        .pin = GPIO_PIN_5, // Based on PC5
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //ANALOG = NOSET
    },
    [HW_GPIO_ADC_MUX_LP8_SNS] = {
        .port = GPIOB,
        .pin = GPIO_PIN_0, // Based on PB0
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //ANALOG = NOSET
    },
    [HW_GPIO_ADC_MUX_LP9_SNS] = {
        .port = GPIOB,
        .pin = GPIO_PIN_1, // Based on PB1
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //ANALOG = NOSET
    },
    [HW_GPIO_MUX2_SEL1] = {
        .port = GPIOE,
        .pin = GPIO_PIN_7, // Based on PE7
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_MUX2_SEL2] = {
        .port = GPIOE,
        .pin = GPIO_PIN_8, // Based on PE8
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_PUMP_EN] = {
        .port = GPIOE,
        .pin = GPIO_PIN_9, // Based on PE9
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_PUMP_FAULT] = {
        .port = GPIOE,
        .pin = GPIO_PIN_10, // Based on PE10
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_FAN_EN] = {
        .port = GPIOE,
        .pin = GPIO_PIN_11, // Based on PE11
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_FAN_FAULT] = {
        .port = GPIOE,
        .pin = GPIO_PIN_12, // Based on PE12
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_SPARE_EN] = {
        .port = GPIOE,
        .pin = GPIO_PIN_13, // Based on PE13
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_BMS4_EN] = {
        .port = GPIOE,
        .pin = GPIO_PIN_14, // Based on PE14
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_OL_DETECT] = {
        .port = GPIOE,
        .pin = GPIO_PIN_15, // Based on PE15
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_VCU_SFTY_RESET] = {
        .port = GPIOB,
        .pin = GPIO_PIN_10, // Based on PB10
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_VCU_SFTY_EN] = {
        .port = GPIOB,
        .pin = GPIO_PIN_11, // Based on PB11
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_CAN2_RX] = {
        .port = GPIOB,
        .pin = GPIO_PIN_12, // Based on PB12
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_CAN2_TX] = {
        .port = GPIOB,
        .pin = GPIO_PIN_13, // Based on PB13
        .mode = GPIO_MODE_AF_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_VC1_EN] = {
        .port = GPIOB,
        .pin = GPIO_PIN_14, // Based on PB14
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_VC2_EN] = {
        .port = GPIOB,
        .pin = GPIO_PIN_15, // Based on PB15
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_LP4_LATCH] = {
        .port = GPIOD,
        .pin = GPIO_PIN_8, // Based on PD8
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_HP_SNS_EN] = {
        .port = GPIOD,
        .pin = GPIO_PIN_9, // Based on PD9
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },  
    [HW_GPIO_DIA_EN] = {
        .port = GPIOD,
        .pin = GPIO_PIN_10, // Based on PD10
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH, 
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_BSPD_MEM] = {
        .port = GPIOD,
        .pin = GPIO_PIN_11, // Based on PD11
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH, 
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_SPI_NCS_SD] = {
        .port = GPIOD,
        .pin = GPIO_PIN_12, // Based on PD12
        .mode = GPIO_MODE_OUTPUT_OD, //OD
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_SHUTDOWN_EN] = {
        .port = GPIOD,
        .pin = GPIO_PIN_13, // Based on PD13
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },  
    [HW_GPIO_MUX_LP_SEL1] = {
        .port = GPIOD,
        .pin = GPIO_PIN_14, // Based on PD14
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_MUX_LP_SEL2] = {
        .port = GPIOD,
        .pin = GPIO_PIN_15, // Based on PD15
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_PWM1] = {
        .port = GPIOC,
        .pin = GPIO_PIN_6, // Based on PC6
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_PWM2] = {
        .port = GPIOC,
        .pin = GPIO_PIN_7, // Based on PC7
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_VCU1_EN] = {
        .port = GPIOC,
        .pin = GPIO_PIN_8, // Based on PC8
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_VCU2_EN] = {
        .port = GPIOC,
        .pin = GPIO_PIN_9, // Based on PC9
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_LP8_LATCH] = {
        .port = GPIOA,
        .pin = GPIO_PIN_8, // Based on PA8
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_UART_EN] = {
        .port = GPIOA,
        .pin = GPIO_PIN_9, // Based on PA9
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_CAN1_RX] = {
        .port = GPIOA,
        .pin = GPIO_PIN_11, // Based on PA11
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_CAN1_TX] = {
        .port = GPIOA,
        .pin = GPIO_PIN_12, // Based on PA12
        .mode = GPIO_MODE_AF_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_SPI_NCS_IMU] = {
        .port = GPIOA,
        .pin = GPIO_PIN_15, // Based on PA15
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_UART_TX_MCU] = {
        .port = GPIOC,
        .pin = GPIO_PIN_10, // Based on PC10
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_UART_RX_MCU] = {
        .port = GPIOC,
        .pin = GPIO_PIN_11, // Based on PC11
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_BMS3_EN] = {
        .port = GPIOC,
        .pin = GPIO_PIN_12, // Based on PC12
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_5V_NFLT1] = {
        .port = GPIOD,
        .pin = GPIO_PIN_0, // Based on PD0
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET, //NOSET(inverse of NEN1)
    },
    [HW_GPIO_HVE_EN] = {
        .port = GPIOD,
        .pin = GPIO_PIN_1, // Based on PD1
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_COCKPIT_EN] = {
        .port = GPIOD,
        .pin = GPIO_PIN_2, // Based on PD2
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_LP6_LATCH] = {
        .port = GPIOD,
        .pin = GPIO_PIN_3, // Based on PD3
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_BMS5_EN] = {
        .port = GPIOD,
        .pin = GPIO_PIN_4, // Based on PD4
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_BMS6_EN] = {
        .port = GPIOD,
        .pin = GPIO_PIN_5, // Based on PD5
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_LP9_LATCH] = {
        .port = GPIOD,
        .pin = GPIO_PIN_6, // Based on PD6
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_LP7_LATCH] = {
        .port = GPIOD,
        .pin = GPIO_PIN_7, // Based on PD7
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_SPI_SCK_MCU] = {
        .port = GPIOB,
        .pin = GPIO_PIN_3, // Based on PB3
        .mode = GPIO_MODE_AF_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_SPI_MISO_MCU] = {
        .port = GPIOB,
        .pin = GPIO_PIN_4, // Based on PB4
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_SPI_MOSI_MCU] = {
        .port = GPIOB,
        .pin = GPIO_PIN_5, // Based on PB5
        .mode = GPIO_MODE_AF_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_SENSOR_EN] = {
        .port = GPIOB,
        .pin = GPIO_PIN_6, // Based on PB6
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_LP3_LATCH] = {
        .port = GPIOB,
        .pin = GPIO_PIN_7, // Based on PB7
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_LED] = {
        .port = GPIOB,
        .pin = GPIO_PIN_8, // Based on PB8
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_LP1_LATCH] = {
        .port = GPIOB,
        .pin = GPIO_PIN_9, // Based on PB9
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_5V_NEN2] = {
        .port = GPIOE,
        .pin = GPIO_PIN_0, // Based on PE0
        .mode = GPIO_MODE_OUTPUT_OD, //OD
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_BMS1_EN] = {
        .port = GPIOE,
        .pin = GPIO_PIN_1, // Based on PE1
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
};