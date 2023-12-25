/**
 * @file HW_Fans.h
 * @brief  Segment Fans Control Driver Header
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-18
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/


#include "HW_tim.h"

#include "HW_Fans.h"

#include "ErrorHandler.h"


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

FANS_State_E current_state = {0};


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initialize fans driver
 */
bool FANS_Init()
{
    bool init_state = true;

    current_state = OFF;
    FANS_SetPower(0);
    
    if (init_state != true) 
    {
        Error_Handler();
        return false;
    }

    return true;
}

/**
 * @brief  Verify fans initialization (Included for forward compatibility)
 *
 * @retval  always true 
 */
bool FANS_Verify()
{
    return true;
}

/**
 * @brief  Returns current state of the fans
 *
 * @retval   current_state of FANS state machine
 *              If the fanse are stopped, the fans must be set to 50% for atleast 0.5s
 *              so that they have time to spin up
 */
FANS_State_E FANS_GetState()
{
    return current_state;
}

/**
 * @brief  Set power output of segment fans
 *
 * @param power Power range [0, 100] in percentage
 */
void FANS_SetPower(uint8_t percentage)
{
    static uint32_t start_time = 0;

    if (percentage == 0)
    {
        current_state = OFF;
        HW_TIM1_setDuty(0);
        return;
    }

    if (current_state == OFF || current_state == STARTING)
    {
        if (current_state == OFF)
        {   current_state = STARTING;
            start_time = HAL_GetTick();
        }
        else if (start_time + 500 < HAL_GetTick())
        {
            current_state = RUNNING;
            start_time = 0;
        }

        HW_TIM1_setDuty((percentage < 50) ? 50 : percentage);
        return;
    }

    /**< Handle non-linearity of optocoupler output for duty-cycle -> response */
    if (percentage > 100)
        percentage = 100;
    else if (percentage <= 10 && percentage > 0)
        percentage = 10;
    else if (percentage < 25 && percentage > 10)
        percentage = 10 + (uint16_t) percentage * 5/25;
    else percentage -= 5;

    HW_TIM1_setDuty(percentage);
}

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

