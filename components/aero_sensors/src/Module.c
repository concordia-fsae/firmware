/**
 * @file Module.c
 * @brief  Source code for Init and Periodic modularized functions
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-23
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Module.h"

#include "SystemConfig.h"
#include "Utility.h"


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static const ModuleDesc_S* modules[] = {
    &Sensors_desc,
};


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Calls Init functions of each module
 */
void Module_Init(void)
{
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->moduleInit != NULL)
        {
            (*modules[i]->moduleInit)();
        }
    }
}

/**
 * @brief  Call the 1KHz periodic function of each module
 */
void Module_1kHz_TSK(void)
{
#if defined (ARS_RTOS_BLINK) /**< Defined in SystemConfig.h */
    static uint8_t tim = 0;
    if (++tim == 100U)
    {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        tim = 0;
    }
#endif /**< ARSDEBUG */
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic1kHz_CLK != NULL)
        {
            (*modules[i]->periodic1kHz_CLK)();
        }
    }
}

/**
 * @brief  Call the 100Hz periodic function of each module
 */
void Module_100Hz_TSK(void)
{
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic100Hz_CLK != NULL)
        {
            (*modules[i]->periodic100Hz_CLK)();
        }
    }
}

/**
 * @brief  Call the 10Hz periodic function of each module
 */
void Module_10Hz_TSK(void)
{
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic10Hz_CLK != NULL)
        {
            (*modules[i]->periodic10Hz_CLK)();
        }
    }
}

/**
 * @brief  Call the 1Hz periodic function of each module
 */
void Module_1Hz_TSK(void)
{
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic1Hz_CLK != NULL)
        {
            (*modules[i]->periodic1Hz_CLK)();
        }
    }
}

