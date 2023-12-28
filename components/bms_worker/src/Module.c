/**
 * @file Module.c
 * @brief  Source code for Module Manager
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-28
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module Header */
#include "Module.h"

/**< System Includes*/
#include "stddef.h"
#include "stdint.h"

/**< Other Includes */
#include "SystemConfig.h"
#include "Utility.h"


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

/**
 * @brief  Modules run by the Module Manager. Order will apply to execution.
 */
static const ModuleDesc_S* modules[] = {
    &BMS_desc,
    &Environment_desc,
    &Cooling_desc,
    &IO_desc,
    &Sys_desc,
//    &CANIO_rx,
//    &CANIO_tx,
};


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Module Init function
 */
void Module_Init(void)
{
    /**< Run each of the modules Init function in order */
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->moduleInit != NULL)
        {
            (*modules[i]->moduleInit)();
        }
    }
}

/**
 * @brief  10kHz periodic function
 */
void Module_10kHz_TSK(void)
{
    /**< Run each of the modules 10kHz function in order */
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic10kHz_CLK != NULL)
        {
            (*modules[i]->periodic10kHz_CLK)();
        }
    }
}

/**
 * @brief  1kHz periodic function
 */
void Module_1kHz_TSK(void)
{
    /**< Run each of the modules 1kHz function in order */
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic1kHz_CLK != NULL)
        {
            (*modules[i]->periodic1kHz_CLK)();
        }
    }
}

/**
 * @brief  100Hz periodic function
 */
void Module_100Hz_TSK(void)
{
    /**< Run each of the modules 100Hz function in order */
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic100Hz_CLK != NULL)
        {
            (*modules[i]->periodic100Hz_CLK)();
        }
    }
}

/**
 * @brief  10Hz periodic function
 */
void Module_10Hz_TSK(void)
{
    /**< Run each of the modules 10Hz function in order */
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic10Hz_CLK != NULL)
        {
            (*modules[i]->periodic10Hz_CLK)();
        }
    }
}

/**
 * @brief  1Hz periodic function
 */
void Module_1Hz_TSK(void)
{
    /**< Run each of the modules 1Hz function in order */
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic1Hz_CLK != NULL)
        {
            (*modules[i]->periodic1Hz_CLK)();
        }
    }
}
