/**
 * @file HW_Fans.h
 * @brief  Segment Fans Control Driver Header
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Inlcudes
#include "HW.h"

// Firmware Includes
#include "HW_Fans.h"
#include "HW_tim.h"

// Other Inlcudes
#include "Cooling.h"


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
 *                           P U B L I C  V A R S
 ******************************************************************************/

FANS_S FANS;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static fans_S fans;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initialize fans driver
 */
bool FANS_init()
{
    for (uint8_t i = 0; i < FAN_COUNT; i++)
    {
        fans.current_state[i] = FANS_OFF;
    }

    return true;
}

/**
 * @brief  Set power output of segment fans
 *
 * @param fan uint8_t[2] with power range [0, 100] in percentage
 */
void FANS_setPower(uint8_t* fan)
{
    FANS_getRPM((uint16_t*)&fans.rpm);

    for (uint8_t i = 0; i < FAN_COUNT; i++)
    {
        if (fan[i] == 0)
        {
            fans.current_state[i] = FANS_OFF;
            fans.percentage[i]    = 0;
        }
        else if ((fans.current_state[i] == FANS_OFF) || (fans.current_state[i] == FANS_STARTING))
        {
            if (fans.current_state[i] == FANS_OFF)
            {
                fans.current_state[i] = FANS_STARTING;
                fans.percentage[i]    = (fans.percentage[i] > 25) ? fans.percentage[i] : 25;
            }
            else if ((fans.current_state[i] == FANS_STARTING) && (fans.rpm[i] > 250))
            {
                fans.current_state[i] = FANS_RUNNING;
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

/**
 * @brief  Returns fanspeed into application provided array
 *
 * @param rpm Pointer to uint8_t[2] for fanspeeds
 */
void FANS_getRPM(uint16_t* rpm)
{
    rpm[0] = HW_TIM1_getFreqCH1() * 60;
    rpm[1] = HW_TIM1_getFreqCH2() * 60;
}
