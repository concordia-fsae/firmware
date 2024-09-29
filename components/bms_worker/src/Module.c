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
#if FEATURE_UDS
    &UDS_desc,
#endif // FEATURE_UDS
#if FEATURE_CANRX_SWI
    &CANIO_rx,
#endif // FEATURE_CANRX_SWI
    &CANIO_tx,
};

static Module_taskStats_S stats[MODULE_TASK_CNT] = { 0 };

static struct
{
    float32_t total;
    float32_t timeslice;
} percentages;

static uint64_t rtos_start;


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

    rtos_start = HW_TIM_getBaseTick();
}

#if FEATURE_10KHZ_TASK
/**
 * @brief  10kHz periodic function
 */
void Module_10kHz_TSK(void)
{
    TaskStatus_t start, finish;

    vTaskGetInfo(NULL, &start, pdFALSE, 0);

    /**< Run each of the modules 10kHz function in order */
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic10kHz_CLK != NULL)
        {
            (*modules[i]->periodic10kHz_CLK)();
        }
    }
    vTaskGetInfo(NULL, &finish, pdFALSE, 0);

    stats[MODULE_10kHz_TASK].total_runtime += finish.ulRunTimeCounter - start.ulRunTimeCounter;
    stats[MODULE_10kHz_TASK].timeslice_runtime += finish.ulRunTimeCounter - start.ulRunTimeCounter;
}
#endif // FEATURE_10KHZ_TASK
/**
 * @brief  1kHz periodic function
 */
void Module_1kHz_TSK(void)
{
    TaskStatus_t start, finish;

    vTaskGetInfo(NULL, &start, pdFALSE, 0);

    /**< Run each of the modules 1kHz function in order */
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic1kHz_CLK != NULL)
        {
            (*modules[i]->periodic1kHz_CLK)();
        }
    }
    vTaskGetInfo(NULL, &finish, pdFALSE, 0);

    stats[MODULE_1kHz_TASK].total_runtime += finish.ulRunTimeCounter - start.ulRunTimeCounter;
    stats[MODULE_1kHz_TASK].timeslice_runtime += finish.ulRunTimeCounter - start.ulRunTimeCounter;
}

/**
 * @brief  100Hz periodic function
 */
void Module_100Hz_TSK(void)
{
    TaskStatus_t start, finish;

    vTaskGetInfo(NULL, &start, pdFALSE, 0);

    /**< Run each of the modules 100Hz function in order */
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic100Hz_CLK != NULL)
        {
            (*modules[i]->periodic100Hz_CLK)();
        }
    }
    vTaskGetInfo(NULL, &finish, pdFALSE, 0);

    stats[MODULE_100Hz_TASK].total_runtime += finish.ulRunTimeCounter - start.ulRunTimeCounter;
    stats[MODULE_100Hz_TASK].timeslice_runtime += finish.ulRunTimeCounter - start.ulRunTimeCounter;
}

/**
 * @brief  10Hz periodic function
 */
void Module_10Hz_TSK(void)
{
    TaskStatus_t start, finish;

    vTaskGetInfo(NULL, &start, pdFALSE, 0);

    /**< Run each of the modules 10Hz function in order */
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic10Hz_CLK != NULL)
        {
            (*modules[i]->periodic10Hz_CLK)();
        }
    }
    vTaskGetInfo(NULL, &finish, pdFALSE, 0);

    stats[MODULE_10Hz_TASK].total_runtime += finish.ulRunTimeCounter - start.ulRunTimeCounter;
    stats[MODULE_10Hz_TASK].timeslice_runtime += finish.ulRunTimeCounter - start.ulRunTimeCounter;
}

/**
 * @brief  1Hz periodic function
 */
void Module_1Hz_TSK(void)
{
    static uint64_t last_timeslice = 0;
    uint64_t        temp_tick;
    TaskStatus_t    start, finish;

    vTaskGetInfo(NULL, &start, pdFALSE, 0);

    /**< Run each of the modules 1Hz function in order */
    for (uint8_t i = 0U; i < COUNTOF(modules); i++)
    {
        if (modules[i]->periodic1Hz_CLK != NULL)
        {
            (*modules[i]->periodic1Hz_CLK)();
        }
    }

    percentages.total     = 0x00;
    percentages.timeslice = 0x00;

    temp_tick = HW_TIM_getBaseTick();

    for (int8_t i = 0; i < MODULE_TASK_CNT; i++)
    {
        stats[i].total_percentage     = (100 * (float32_t)stats[i].total_runtime) / (float32_t)(temp_tick - rtos_start);
        stats[i].timeslice_percentage = (100 * (float32_t)stats[i].timeslice_runtime) / (float32_t)(temp_tick - last_timeslice);
        percentages.total += stats[i].total_percentage;
        percentages.timeslice += stats[i].timeslice_percentage;
        stats[i].timeslice_runtime = 0;
    }

    last_timeslice = temp_tick;

    vTaskGetInfo(NULL, &finish, pdFALSE, 0);

    stats[MODULE_1Hz_TASK].total_runtime += finish.ulRunTimeCounter - start.ulRunTimeCounter;
    stats[MODULE_1Hz_TASK].timeslice_runtime += finish.ulRunTimeCounter - start.ulRunTimeCounter;
}

/**
 * @brief  Idle task used by FreeRTOS
 */
void Module_ApplicationIdleHook()
{
}
