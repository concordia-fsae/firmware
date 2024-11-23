/**
 * @file Cooling.c
 * @brief  Source code for Cooling Application
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-27
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module header */
#include "Cooling.h"

/**< Other Includes */
#include "Environment.h"
#include "Module.h"

#include "MessageUnpack_generated.h"


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

COOL_S COOL;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Cooling Module Init function
 */
static void Cooling_Init()
{
    for (uint8_t i = 0; i < FAN_COUNT; i++)
    {
        COOL.state[i]      = COOL_INIT;
        COOL.percentage[i] = 0;
        COOL.rpm[i]        = 0;
    }
    FANS_init();
}

/**
 * @brief  Cooling Module 1Hz periodic function
 */
static void Cooling10Hz_PRD(void)
{
    static uint8_t step = 0;

    FANS_getRPM((uint16_t*)&COOL.rpm);

    for (uint8_t i = 0; i < FAN_COUNT; i++)
    {
        switch (COOL.state[i])
        {
            case COOL_INIT:
                step              += 2;
                COOL.percentage[i] = (step <= 100) ? step : 200 - step;

                if (step >= 200)
                {
                    COOL.state[i] = COOL_ON;
                }
                break;

            case COOL_ON:
            case COOL_OFF:
                if (ENV.state == ENV_FAULT)
                {
                    COOL.state[i]      = COOL_FULL;
                    COOL.percentage[i] = 100;
                    return;
                }
                if (ENV.values.max_temp < 35)
                {
                    COOL.percentage[i] = 0;
                }
                else if (ENV.values.max_temp > 55)
                {
                    COOL.state[i]      = COOL_FULL;
                    COOL.percentage[i] = 100;
                }
                else
                {
                    COOL.state[i]      = COOL_ON;
                    COOL.percentage[i] = (uint8_t)(((ENV.values.max_temp - 40) * 100) / 20);
                }
                break;

            case COOL_FULL:
                if (ENV.values.max_temp < 52 && ENV.state == ENV_RUNNING)
                {
                    COOL.state[i] = COOL_ON;
                }
                break;

            case COOL_ERR:
                break;

            default:
                break;
        }


        if ((uint8_t)COOL.percentage[i] == 0)
        {
            COOL.state[i] = COOL_OFF;
        }

        uint8_t percent_beans = 0;
        if (CANRX_get_signal(VEH, TOOLING_commandedFansDutyCycle, &percent_beans) == CANRX_MESSAGE_VALID)
        {
            COOL.percentage[i] = percent_beans;
        }
    }

    FANS_setPower((uint8_t*)&COOL.percentage);
}

/**
 * @brief  Cooling Module descriptor
 */
const ModuleDesc_S COOL_desc = {
    .moduleInit       = &Cooling_Init,
    .periodic10Hz_CLK = &Cooling10Hz_PRD,
};
