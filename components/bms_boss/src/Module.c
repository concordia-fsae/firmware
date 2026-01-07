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
#include "lib_utility.h"
#include "FeatureDefines_generated.h"
#include "lib_nvm.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

/**
 * @brief  Modules run by the Module Manager. Order will apply to execution.
 */
static const ModuleDesc_S* modules[] = {
    &drv_inputAD_desc,
    &ENV_desc,
    &BMS_desc,
    &SYS_desc,
#if APP_UDS
    &UDS_desc,
#endif
    &CANIO_rx,
    &CANIO_tx,
    &socEstimation_desc
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

    stats[MODULE_1kHz_TASK].total_percentage = (uint8_t)ulTaskGetRunTimePercent(NULL);
    stats[MODULE_1kHz_TASK].iterations++;
    stats[MODULE_1kHz_TASK].stack_left = (uint16_t)uxTaskGetStackHighWaterMark(NULL);
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

    stats[MODULE_100Hz_TASK].total_percentage = (uint8_t)ulTaskGetRunTimePercent(NULL);
    stats[MODULE_100Hz_TASK].iterations++;
    stats[MODULE_100Hz_TASK].stack_left = (uint16_t)uxTaskGetStackHighWaterMark(NULL);
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

    stats[MODULE_10Hz_TASK].total_percentage = (uint8_t)ulTaskGetRunTimePercent(NULL);
    stats[MODULE_10Hz_TASK].iterations++;
    stats[MODULE_10Hz_TASK].stack_left = (uint16_t)uxTaskGetStackHighWaterMark(NULL);
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

    stats[MODULE_1Hz_TASK].total_percentage = (uint8_t)ulTaskGetRunTimePercent(NULL);
    stats[MODULE_1Hz_TASK].iterations++;
    stats[MODULE_1Hz_TASK].stack_left = (uint16_t)uxTaskGetStackHighWaterMark(NULL);
}

/**
 * @brief  Idle task used by FreeRTOS
 */
void Module_ApplicationIdleHook()
{
    stats[MODULE_IDLE_TASK].total_percentage = (uint8_t)ulTaskGetRunTimePercent(NULL);
    stats[MODULE_IDLE_TASK].iterations++;
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

/**
 * @brief Returns the total iterations of a task
 * @param task Task to get the total runtime iterations of
 * @returns The total runtime iterations of the task
 */
uint32_t Module_getTotalRuntimeIterations(Module_taskSpeeds_E task)
{
    return stats[task].iterations;
}

/**
 * @brief Return the minimum lifetime stack bytes left
 * @param task Task to get the total runtime iterations of
 * @returns The total runtime stack left saturated from 0 to 256 bytes
 */
uint8_t Module_getMinStackLeft(Module_taskSpeeds_E task)
{
    return (uint8_t)SATURATE(0, stats[task].stack_left, 256);
}
