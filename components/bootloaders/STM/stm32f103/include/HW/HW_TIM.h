/*
 * HW_TIM.h
 * This file desribes low-level, mostly hardware-specific Timer peripheral values
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Types.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define TIM2_BASE        (0x40000000UL)

// Timer 2 registers
#define TIM2_CR1         (TIM2_BASE + 0x00UL) // Control Register 1
#define TIM2_DIER        (TIM2_BASE + 0x0CUL) // DMA/Interrupt Enable Register
#define TIM2_SR          (TIM2_BASE + 0x10UL) // Status Register
#define TIM2_CNT         (TIM2_BASE + 0x24UL) // Counter Register
#define TIM2_PSC         (TIM2_BASE + 0x28UL) // Prescaler Register
#define TIM2_ARR         (TIM2_BASE + 0x2CUL) // Auto Reload Register

// Timer 2 bits
#define TIM2_CR1_CE      (0x01UL)  // Counter Enable
#define TIM2_CR1_DIR     (0x10UL)  // Counter Direction (0 = up, 1 = down)
#define TIM2_SR_UIF      (0x01UL)  // Update Interrupt Flag
#define TIM2_DIER_UIE    (0x01UL)  // Update Interrupt Enable


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void   TIM_init(void);
Time_t TIM_getTimeMs(void);
