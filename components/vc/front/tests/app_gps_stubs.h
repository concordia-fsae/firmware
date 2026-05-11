#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "HW_gpio_componentSpecific.h"

#define FEATURE_DISABLED 0U
#define FEATURE_ENABLED 1U

#define FEATURE_GPSTRANSCEIVER FEATURE_ENABLED
#define MCU_STM32_USE_HAL FEATURE_DISABLED

#define FEATURE_IS_ENABLED(feature) ((feature) == FEATURE_ENABLED)
#define FEATURE_IS_DISABLED(feature) ((feature) == FEATURE_DISABLED)

#define taskENTER_CRITICAL() \
    do \
    { \
    } while (0)

#define taskEXIT_CRITICAL() \
    do \
    { \
    } while (0)

#define HW_UART_PORT_GPS 0U

#define HAL_UART_ERROR_ORE (1UL << 0U)
#define HAL_UART_ERROR_FE (1UL << 1U)
#define HAL_UART_ERROR_NE (1UL << 2U)
#define HAL_UART_ERROR_PE (1UL << 3U)

static inline void HW_UART_startDMARX(uint32_t port, uint32_t* buffer, uint32_t length)
{
    (void)port;
    (void)buffer;
    (void)length;
}

static inline void HW_UART_stopDMA(uint32_t port)
{
    (void)port;
}

static inline uint32_t HW_TIM_getTimeMS(void)
{
    return 0U;
}

static inline bool HW_GPIO_readPin(HW_GPIO_pinmux_E pin)
{
    (void)pin;
    return false;
}

static inline void HW_GPIO_writePin(HW_GPIO_pinmux_E pin, bool state)
{
    (void)pin;
    (void)state;
}

static inline void HW_GPIO_togglePin(HW_GPIO_pinmux_E pin)
{
    (void)pin;
}
