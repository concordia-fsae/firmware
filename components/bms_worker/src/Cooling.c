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

/**< Driver Includes */
#include "HW_Fans.h"

/**< Other Includes */
#include "Environment.h"
#include "Module.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern Environment_S ENV;

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
    COOLING.state      = COOLING_INIT;
    COOLING.percentage = 0;
    FANS_Init();

    COOLING.state = COOLING_INIT;
}

/**
 * @brief  Cooling Module 1Hz periodic function
 */
static void Cooling1Hz_PRD(void)
{
    static uint8_t step = 0;

    switch (COOLING.state)
    {
        case COOLING_INIT:
            step += 20;
            COOLING.percentage = (step <= 100) ? step : 200 - step;

            if (step >= 200)
                COOLING.state = COOLING_OFF;
            break;
        case COOLING_ON:
        case COOLING_OFF:
            if (ENV.values.cells.max_temp < 350)
            {
                COOLING.percentage = 0;
            }

#include "Cooling.h"
            else if (ENV.values.cells.max_temp > 550)
            {
                COOLING.state = COOLING_FULL;
                COOLING.percentage = 100;
            }
            else if (ENV.values.cells.max_temp > 500)
            {
                COOLING.state = COOLING_ON;
                COOLING.percentage = 80;
            }
            else if (ENV.values.cells.max_temp > 450)
            {
                COOLING.state = COOLING_ON;
                COOLING.percentage = 60;
            }
            else if (ENV.values.cells.max_temp > 400)
            {
                COOLING.state = COOLING_ON;
                COOLING.percentage = 40;
            }
            else if (ENV.values.cells.max_temp >= 350)
            {
                COOLING.state = COOLING_ON;
                COOLING.percentage = 20;
            }
            break;
        case COOLING_FULL:
            if (ENV.values.cells.max_temp < 525)
                COOLING.state = COOLING_ON;
            break;
        case COOLING_ERR:
            break;
        default:
            break;
    }


    if (COOLING.percentage == 0)
    {
        COOLING.state = COOLING_OFF;
    }

    FANS_SetPower(COOLING.percentage);
}


/**
 * @brief  Cooling Module descriptor
 */
const ModuleDesc_S Cooling_desc = {
    .moduleInit      = &Cooling_Init,
    .periodic1Hz_CLK = &Cooling1Hz_PRD,
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/
