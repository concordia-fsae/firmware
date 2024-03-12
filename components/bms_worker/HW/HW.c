/**
 * @file HW.c
 * @brief  Source code for generic firmware functions
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Header file
#include "HW.h"

// System Includes
#include "stdbool.h"

// Firmware Includes
#include "stm32f1xx.h"
#include "include/HW_tim.h"


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * @brief  Initializes the generic low-level firmware
 *
 * @retval HW_OK  
 */
HW_StatusTypeDef_E HW_init(void) 
{
    HAL_Init();
    return HW_OK;
}

/**
 * @brief Deinitializes the generic low-level firmware
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_deInit(void)
{
    HAL_DeInit();
    return HW_OK;
}

/**
 * @brief  Get the number of ticks since clock start
 *
 * @retval Number of ticks
 */
uint32_t HW_getTick(void)
{
    return HAL_GetTick();
}

/**
 * @brief  Delay the execution in blocking mode for amount of ticks
 *
 * @param delay Number of ticks to delay in blocking mode
 */
void HW_delay(uint32_t delay)
{
    HAL_Delay(delay);
}

/**
 * @brief  This function is blocking and should be avoided
 *
 * @param us Microsecond blocking delay
 */
void HW_usDelay(uint8_t us)
{
    uint64_t us_start = HW_TIM_getBaseTick();

    while (HW_TIM_getBaseTick() < us_start + us);
}
