#pragma once

#include <stdint.h>

typedef struct
{
    uint32_t unused;
} GPIO_TypeDef;

typedef struct
{
    uint32_t unused;
} TIM_HandleTypeDef;

typedef struct
{
    uint32_t unused;
} ADC_HandleTypeDef;

typedef struct
{
    uint32_t unused;
} CAN_HandleTypeDef;

typedef struct
{
    uint32_t unused;
} UART_HandleTypeDef;

typedef struct
{
    uint32_t unused;
} DMA_HandleTypeDef;

typedef struct
{
    uint32_t PLLState;
    uint32_t PLLSource;
    uint32_t PLLMUL;
} RCC_PLLInitTypeDef;

typedef struct
{
    uint32_t           OscillatorType;
    uint32_t           HSEState;
    uint32_t           HSEPredivValue;
    uint32_t           HSIState;
    RCC_PLLInitTypeDef PLL;
} RCC_OscInitTypeDef;

typedef struct
{
    uint32_t ClockType;
    uint32_t SYSCLKSource;
    uint32_t AHBCLKDivider;
    uint32_t APB1CLKDivider;
    uint32_t APB2CLKDivider;
} RCC_ClkInitTypeDef;

typedef struct
{
    uint32_t PeriphClockSelection;
    uint32_t AdcClockSelection;
} RCC_PeriphCLKInitTypeDef;

#define HAL_OK                     0
#define HAL_UART_ERROR_PE          0x00000001U
#define HAL_UART_ERROR_NE          0x00000002U
#define HAL_UART_ERROR_FE          0x00000004U
#define HAL_UART_ERROR_ORE         0x00000008U
#define RCC_OSCILLATORTYPE_HSE     0x00000001U
#define RCC_HSE_ON                 1U
#define RCC_HSE_PREDIV_DIV1        1U
#define RCC_HSI_ON                 1U
#define RCC_PLL_ON                 1U
#define RCC_PLLSOURCE_HSE          1U
#define RCC_PLL_MUL8               8U
#define RCC_CLOCKTYPE_HCLK         0x00000001U
#define RCC_CLOCKTYPE_SYSCLK       0x00000002U
#define RCC_CLOCKTYPE_PCLK1        0x00000004U
#define RCC_CLOCKTYPE_PCLK2        0x00000008U
#define RCC_SYSCLKSOURCE_PLLCLK    1U
#define RCC_SYSCLK_DIV1            1U
#define RCC_HCLK_DIV1              1U
#define RCC_HCLK_DIV2              2U
#define RCC_PERIPHCLK_ADC          1U
#define RCC_ADCPCLK2_DIV8          8U
#define FLASH_LATENCY_2            2U

static inline int HAL_RCC_OscConfig(RCC_OscInitTypeDef* config)
{
    (void)config;
    return HAL_OK;
}

static inline int HAL_RCC_ClockConfig(RCC_ClkInitTypeDef* config, uint32_t latency)
{
    (void)config;
    (void)latency;
    return HAL_OK;
}

static inline int HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef* config)
{
    (void)config;
    return HAL_OK;
}

static inline void __disable_irq(void)
{
}
