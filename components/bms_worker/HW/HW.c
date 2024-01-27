/**
 * @file HW.c
 * @brief  Source code for generic firmware functions
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-28
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Header file */
#include "HW.h"

/**< System Includes */
#include "include/HW_tim.h"
#include "stdbool.h"

/**< Firmware Includes */
#include "stm32f1xx.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * @brief  Initializes the generic low-level firmware
 *
 * @retval Always true 
 */
bool HW_Init() 
{
    return HAL_Init() == HAL_OK;
}

/**
 * @brief  Get the number of ticks since clock start
 *
 * @retval Number of ticks
 */
uint32_t HW_GetTick()
{
    return HAL_GetTick();
}

/**
 * @brief  Delay the execution in blocking mode for amount of ticks
 *
 * @param delay Number of ticks to delay in blocking mode
 */
void HW_Delay(uint32_t delay)
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
    uint64_t us_start = HW_TIM_GetBaseTick();

    while (HW_TIM_GetBaseTick() < us_start + us);
}

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/
