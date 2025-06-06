/**
 * @file HW_gpio_componentSpecific.c
 * @brief  Source code for GPIO firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW_gpio.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

const HW_GPIO_S HW_GPIO_pinmux[HW_GPIO_COUNT] = {
    [HW_GPIO_DIG_SPARE1] = {
        .port = GPIOE,
        .pin = GPIO_PIN_2,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_DIG_SPARE2] = {
        .port = GPIOE,
        .pin = GPIO_PIN_3,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_DIG_SPARE3] = {
        .port = GPIOE,
        .pin = GPIO_PIN_4,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_DIG_SPARE4] = {
        .port = GPIOE,
        .pin = GPIO_PIN_5,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_LED] = {
        .port = GPIOC,
        .pin = GPIO_PIN_13,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_ADC_SPARE1] = {
        .port = GPIOC,
        .pin = GPIO_PIN_0,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_SPARE2] = {
        .port = GPIOC,
        .pin = GPIO_PIN_1,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_SPARE3] = {
        .port = GPIOC,
        .pin = GPIO_PIN_2,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_SPARE4] = {
        .port = GPIOC,
        .pin = GPIO_PIN_3,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_APPS_P1] = {
        .port = GPIOA,
        .pin = GPIO_PIN_1,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_APPS_P2] = {
        .port = GPIOA,
        .pin = GPIO_PIN_2,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_BR_POT] = {
        .port = GPIOA,
        .pin = GPIO_PIN_3,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_L_SHK_DISP] = {
        .port = GPIOA,
        .pin = GPIO_PIN_4,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_L_BR_TEMP] = {
        .port = GPIOA,
        .pin = GPIO_PIN_5,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_R_SHK_DISP] = {
        .port = GPIOA,
        .pin = GPIO_PIN_6,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_R_BR_TEMP] = {
        .port = GPIOA,
        .pin = GPIO_PIN_7,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_PU1] = {
        .port = GPIOC,
        .pin = GPIO_PIN_4,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_PU2] = {
        .port = GPIOC,
        .pin = GPIO_PIN_5,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_BR_PR] = {
        .port = GPIOB,
        .pin = GPIO_PIN_0,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_ADC_TEMP_1] = {
        .port = GPIOB,
        .pin = GPIO_PIN_1,
        .mode = GPIO_MODE_ANALOG,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_5V_NFLT2] = {
        .port = GPIOE,
        .pin = GPIO_PIN_7,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_5V_NEN2] = {
        .port = GPIOE,
        .pin = GPIO_PIN_8,
        .mode = GPIO_MODE_OUTPUT_OD,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINSET,
    },
    [HW_GPIO_5V_NEN1] = {
        .port = GPIOE,
        .pin = GPIO_PIN_9,
        .mode = GPIO_MODE_OUTPUT_OD,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINSET,
    },
    [HW_GPIO_5V_NFLT1] = {
        .port = GPIOE,
        .pin = GPIO_PIN_10,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_CAN2_RX] = {
        .port = GPIOB,
        .pin = GPIO_PIN_12,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_CAN2_TX] = {
        .port = GPIOB,
        .pin = GPIO_PIN_13,
        .mode = GPIO_MODE_AF_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_MUX_SEL2] = {
        .port = GPIOD,
        .pin = GPIO_PIN_9,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_MUX_SEL1] = {
        .port = GPIOD,
        .pin = GPIO_PIN_10,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_HORN_EN] = {
        .port = GPIOD,
        .pin = GPIO_PIN_14,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_BMS_LIGHT_EN] = {
        .port = GPIOC,
        .pin = GPIO_PIN_7,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_IMD_LIGHT_EN] = {
        .port = GPIOC,
        .pin = GPIO_PIN_6,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_TSSI_G_EN] = {
        .port = GPIOD,
        .pin = GPIO_PIN_15,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_TSSI_R_EN] = {
        .port = GPIOC,
        .pin = GPIO_PIN_8,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_BR_LIGHT_EN] = {
        .port = GPIOC,
        .pin = GPIO_PIN_9,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_CAN1_RX] = {
        .port = GPIOA,
        .pin = GPIO_PIN_11,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_CAN1_TX] = {
        .port = GPIOA,
        .pin = GPIO_PIN_12,
        .mode = GPIO_MODE_AF_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_MCU_UART_EN] = {
        .port = GPIOA,
        .pin = GPIO_PIN_15,
        .mode = GPIO_MODE_OUTPUT_PP,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_MCU_UART_TX] = {
        .port = GPIOC,
        .pin = GPIO_PIN_10,
        .mode = GPIO_MODE_AF_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_MCU_UART_RX] = {
        .port = GPIOC,
        .pin = GPIO_PIN_11,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_SPI_NCS_IMU] = {
        .port = GPIOD,
        .pin = GPIO_PIN_2,
        .mode = GPIO_MODE_OUTPUT_OD,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_SPI_NCS_SD] = {
        .port = GPIOD,
        .pin = GPIO_PIN_1,
        .mode = GPIO_MODE_OUTPUT_OD,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_PINRESET,
    },
    [HW_GPIO_SPI_SCK] = {
        .port = GPIOB,
        .pin = GPIO_PIN_3,
        .mode = GPIO_MODE_AF_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_SPI_MISO] = {
        .port = GPIOB,
        .pin = GPIO_PIN_4,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_SPI_MOSI] = {
        .port = GPIOB,
        .pin = GPIO_PIN_5,
        .mode = GPIO_MODE_AF_PP,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_TACH_FLOW] = {
        .port = GPIOE,
        .pin = GPIO_PIN_0,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_TACH_WHEELSPEED_R] = {
        .port = GPIOB,
        .pin = GPIO_PIN_8,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_TACH_WHEELSPEED_L] = {
        .port = GPIOB,
        .pin = GPIO_PIN_9,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_HIGH,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
    [HW_GPIO_RUN_BUTTON] = {
        .port = GPIOE,
        .pin = GPIO_PIN_1,
        .mode = GPIO_MODE_INPUT,
        .speed = GPIO_SPEED_FREQ_LOW,
        .pull = GPIO_NOPULL,
        .resetState = HW_GPIO_NOSET,
    },
};