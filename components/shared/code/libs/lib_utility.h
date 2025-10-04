/**
 * @file lib_utility.h
 * @brief  Header file for Thermistor Library
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define SATURATE(min, x, max) ((x < min) ? min : ((x > max) ? max : x))
#define COUNTOF(x) (sizeof(x)/sizeof(x[0U]))

// taken from rogerclark
#define SET_REG(addr, val)       do { *(volatile uint32_t*)(addr) = val; } while (0)
#define GET_REG(addr)            (*(volatile uint32_t*)(addr))

#if FEATURE_IS_ENABLED(MCU_STM32_USE_HAL)
#include "stm32f1xx.h"
#elif FEATURE_IS_DISABLED(MCU_STM32_USE_HAL)
// taken from stm32 hal
#define SET_BIT(REG, BIT)                      ((REG) |= (BIT))
#define CLEAR_BIT(REG, BIT)                    ((REG) &= ~(BIT))
#define READ_BIT(REG, BIT)                     ((REG)&(BIT))
#define CLEAR_REG(REG)                         ((REG) = (0x0))
#define WRITE_REG(REG, VAL)                    ((REG) = (VAL))
#define READ_REG(REG)                          ((REG))
#define MODIFY_REG(REG, CLEARMASK, SETMASK)    WRITE_REG((REG), (((READ_REG(REG)) & (~(CLEARMASK))) | (SETMASK)))
#endif

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

float32_t ln(float32_t x);
