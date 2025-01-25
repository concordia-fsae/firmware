/**
 * @file Module.c
 * @brief  Source code for Module Manager
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Module Header */
#include "Module.h"

/**< System Includes*/
#include "SystemConfig.h"
#include "stddef.h"
#include "stdint.h"

// FreeRTOS Includes
#include "FreeRTOS.h"
#include "task.h"

/**< Other Includes */
#include "Utility.h"
#include "FeatureDefines_generated.h"


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

/**
 * @brief  Modules run by the Module Manager. Order will apply to execution.
 */
static const ModuleDesc_S* modules[] = {
    &BMS_desc,
    &ENV_desc,
    &COOL_desc,
    &IO_desc,
    &SYS_desc,
#if FEATURE_IS_ENABLED(APP_UDS)
    &UDS_desc,
#endif // FEATURE_UDS
    &CANIO_rx,
    &CANIO_tx,
};

static Module_taskStats_S stats[MODULE_TASK_CNT] = { 0 };

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

#if FEATURE_IS_ENABLED(APP_10KHZ_TASK)
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

    stats[MODULE_10kHz_TASK].total_percentage = (float32_t)ulTaskGetRunTimePercent(NULL);
}
#endif // FEATURE_10KHZ_TASK

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

    stats[MODULE_1kHz_TASK].total_percentage = (float32_t)ulTaskGetRunTimePercent(NULL);
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

    stats[MODULE_100Hz_TASK].total_percentage = (float32_t)ulTaskGetRunTimePercent(NULL);
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

    stats[MODULE_10Hz_TASK].total_percentage = (float32_t)ulTaskGetRunTimePercent(NULL);
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

    stats[MODULE_1Hz_TASK].total_percentage = (float32_t)ulTaskGetRunTimePercent(NULL);
}

/**
 * @brief  Idle task used by FreeRTOS
 */
void Module_ApplicationIdleHook()
{
    stats[MODULE_IDLE_TASK].total_percentage = (float32_t)ulTaskGetRunTimePercent(NULL);
}

/**
 * @brief Returns the total cpu usage of a task in percentage
 * @param task Task to get the total runtime percentage of
 * @returns The total runtime percentage of the task
 */
float32_t Module_getTotalRuntimePercentage(Module_taskSpeeds_E task)
{
    return stats[task].total_percentage;
}
