/*
 * Module.c
 */

#include "stddef.h"
#include "stdint.h"

// includes
#include "Module.h"
#include "Utility.h"


static const ModuleDesc_S* modules[] = {
    &Screen_desc,
};


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
