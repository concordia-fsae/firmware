/**
 * @file Module.h
 * @brief  Header file for Module Manager
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "stdint.h"
#include "FloatTypes.h"

// Other Includes
#include "ModuleDesc.h"
#include "FeatureDefines_generated.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/**< Modules */
extern const ModuleDesc_S BMS_desc;
extern const ModuleDesc_S COOL_desc;
extern const ModuleDesc_S ENV_desc;
extern const ModuleDesc_S SYS_desc;
extern const ModuleDesc_S IO_desc;
extern const ModuleDesc_S UDS_desc;
extern const ModuleDesc_S CANIO_rx;
extern const ModuleDesc_S CANIO_tx;

/**< Module tasks to get called by the RTOS */
extern void Module_Init(void);
#if FEATURE_IS_ENABLED(APP_10KHZ_TASK)
extern void Module_10kHz_TSK(void);
#endif // APP_10KHZ_TASK
extern void Module_1kHz_TSK(void);
extern void Module_100Hz_TSK(void);
extern void Module_10Hz_TSK(void);
extern void Module_1Hz_TSK(void);


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    MODULE_1Hz_TASK = 0x00,
    MODULE_10Hz_TASK,
    MODULE_100Hz_TASK,
    MODULE_1kHz_TASK,
#if FEATURE_IS_ENABLED(APP_10KHZ_TASK)
    MODULE_10kHz_TASK,
#endif // APP_10KHZ_TASK
    MODULE_IDLE_TASK,
    MODULE_TASK_CNT
} Module_taskSpeeds_E;

typedef struct
{
    float32_t total_percentage;
    uint32_t  iterations;
} Module_taskStats_S;

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t Module_getTotalRuntimePercentage(Module_taskSpeeds_E task);
uint32_t  Module_getTotalRuntimeIterations(Module_taskSpeeds_E task);
void Module_ApplicationIdleHook(void);
