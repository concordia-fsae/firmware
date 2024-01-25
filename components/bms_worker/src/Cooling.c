/**
 * @file Cooling.c
 * @brief  Source code for Cooling Manager
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

/**
 * @brief  Stores the public Cooling Manager struct
 */
Cooling_Mngr_S COOLING;


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
 * @brief  Cooling Module Init function
 */
static void Cooling_Init()
{
    for (uint8_t i = 0; i < FAN_COUNT; i++)
    {
        COOLING.state[i]      = COOLING_INIT;
        COOLING.percentage[i] = 0;
        COOLING.rpm[i]        = 0;
    }
    FANS_Init();
}

/**
 * @brief  Cooling Module 1Hz periodic function
 */
static void Cooling10Hz_PRD(void)
{
    static uint8_t step = 0;

    for (uint8_t i = 0; i < FAN_COUNT; i++)
    {
        switch (COOLING.state[i])
        {
            case COOLING_INIT:
                step += 5;
                COOLING.percentage[i] = (step <= 100) ? step : 200 - step;

                if (step >= 200)
                    COOLING.state[i] = COOLING_OFF;
                break;
            case COOLING_ON:
            case COOLING_OFF:
                if (ENV.values.cells.max_temp < 350)
                {
                    COOLING.percentage[i] = 0;
                }
                else if (ENV.values.cells.max_temp > 550)
                {
                    COOLING.state[i]      = COOLING_FULL;
                    COOLING.percentage[i] = 100;
                }
                else if (ENV.values.cells.max_temp > 500)
                {
                    COOLING.state[i]      = COOLING_ON;
                    COOLING.percentage[i] = 80;
                }
                else if (ENV.values.cells.max_temp > 450)
                {
                    COOLING.state[i]      = COOLING_ON;
                    COOLING.percentage[i] = 60;
                }
                else if (ENV.values.cells.max_temp > 400)
                {
                    COOLING.state[i]      = COOLING_ON;
                    COOLING.percentage[i] = 40;
                }
                else if (ENV.values.cells.max_temp >= 350)
                {
                    COOLING.state[i]      = COOLING_ON;
                    COOLING.percentage[i] = 20;
                }
                break;
            case COOLING_FULL:
                if (ENV.values.cells.max_temp < 525)
                    COOLING.state[i] = COOLING_ON;
                break;
            case COOLING_ERR:
                break;
            default:
                break;
        }


        if (COOLING.percentage[i] == 0)
        {
            COOLING.state[i] = COOLING_OFF;
        }
    }

    FANS_GetRPM((uint16_t*)&COOLING.rpm);
    FANS_SetPower((uint8_t*)&COOLING.percentage);    // COOLING.percentage);
}


/**
 * @brief  Cooling Module descriptor
 */
const ModuleDesc_S Cooling_desc = {
    .moduleInit       = &Cooling_Init,
    .periodic10Hz_CLK = &Cooling10Hz_PRD,
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/
