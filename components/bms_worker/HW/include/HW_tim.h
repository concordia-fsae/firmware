/**
 * HW_timebase.c
 * Hardware timer and tick config
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stm32f1xx_hal.h"


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * HAL_InitTick
 * This function configures the TIM4 as a time base source.
 * The time source is configured  to have 1ms time base with a dedicated
 * Tick interrupt priority.
 * @note   This function is called  automatically at the beginning of program after
 *         reset by HAL_Init() or at any time when clock is configured, by HAL_RCC_ClockConfig().
 * @param  TickPriority Tick interrupt priority.
 * @return exit status
 */
HAL_StatusTypeDef HW_TIM_Init(void);
void HW_TIM_ConfigureRunTimeStatsTimer(void);
void HW_TIM_IncBaseTick(void);
uint64_t HW_TIM_GetBaseTick(void);

#if defined (BMSW_BOARD_VA1)
void HW_TIM1_setDuty(uint8_t);
#elif defined (BMSW_BOARD_VA3) /**< BMSW_BOARD_VA1 */
void HW_TIM4_setDuty(uint8_t, uint8_t);
#endif

