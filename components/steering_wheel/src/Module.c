/**
 * Module.c
 * Define all the modules in this project
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stddef.h"
#include "stdint.h"

#include "Module.h"

#include "SystemConfig.h"

#include "Utility.h"


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static const ModuleDesc_S* modules[] = {
    &IO_desc,
    &CANIO_rx,
    &CANIO_tx,
    &Screen_desc,
};


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/*
 * Call the init function for each module
 */
void Module_init(void)
{
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->moduleInit != NULL)
        {
            (*modules[i]->moduleInit)();
        }
    }
}

/*
 * Call the 1kHz periodic function for each module
 */
void Module_1kHz_TSK(void)
{
    // static uint8_t tim = 0;
    // if (++tim == 100U)
    // {
    //     HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    //     tim = 0;
    // }
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic1kHz_CLK != NULL)
        {
            (*modules[i]->periodic1kHz_CLK)();
        }
    }
}

/*
 * Call the 100Hz periodic function for each module
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

/*
 * Call the 10Hz periodic function for each module
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

/*
 * Call the 1Hz periodic function for each module
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
