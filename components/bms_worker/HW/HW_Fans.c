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

#include "Cooling.h"
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

typedef struct
{
    FANS_State_E current_state[FAN_COUNT];
    uint8_t      percentage[FAN_COUNT];
    uint16_t     rpm[FAN_COUNT];
} fans_S;


/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static fans_S fans;


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

    for (uint8_t i = 0; i < FAN_COUNT; i++)
    {
        fans.current_state[i] = OFF;
    }

    if (init_state != true)
    {
        Error_Handler();
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
 * @brief  Set power output of segment fans
 *
 * @param power Power range [0, 100] in percentage
 */
void FANS_SetPower(uint8_t* fan)
{
    FANS_GetRPM((uint16_t*)&fans.rpm);

    for (uint8_t i = 0; i < FAN_COUNT; i++)
    {
        if (fan[i] == 0)
        {
            fans.current_state[i] = OFF;
            fans.percentage[i]    = 0;
        }
        else if (fans.current_state[i] == OFF || fans.current_state[i] == STARTING)
        {
            if (fans.current_state[i] == OFF)
            {
                fans.current_state[i] = STARTING;
                fans.percentage[i]    = (fans.percentage[i] > 25) ? fans.percentage[i] : 25;
            }
            else if (fans.current_state[i] == STARTING && fans.rpm[i] > 250)
            {
                fans.current_state[i] = RUNNING;
                fans.percentage[i]    = fan[i];
            }
        }
        else
        {
            fans.percentage[i] = fan[i];
        }
    }

    HW_TIM4_setDutyCH1(fans.percentage[FAN1]);
    HW_TIM4_setDutyCH2(fans.percentage[FAN2]);
}

void FANS_GetRPM(uint16_t* rpm)
{
    rpm[0] = HW_TIM1_getFreqCH1() * 60;
    rpm[1] = HW_TIM1_getFreqCH2() * 60;
}

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/
