/*
 * BuildDefines.h
 *
 */

#pragma once

#include "FeatureDefines_generated.h"

#if (MCU_STM32_PN == FDEFS_STM32_PN_STM32F103XB)
#define STM32F1
#define STM32F103xB
#if FEATURE_IS_ENABLED(MCU_STM32_USE_HAL)
#define USE_HAL_DRIVER
#endif // stm32f103xb
