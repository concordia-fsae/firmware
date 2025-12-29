/**
 * @file Module.h
 * @brief  Header file for Module Manager
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "FloatTypes.h"
#include "stdint.h"

// Other Includes
#include "ModuleDesc.h"


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/**< Modules */
extern const ModuleDesc_S BMS_desc;
extern const ModuleDesc_S ENV_desc;
extern const ModuleDesc_S SYS_desc;
extern const ModuleDesc_S drv_inputAD_desc;
extern const ModuleDesc_S UDS_desc;
extern const ModuleDesc_S CANIO_rx;
extern const ModuleDesc_S CANIO_tx;
extern const ModuleDesc_S BMS_Alarm_desc;

/**< Module tasks to get called by the RTOS */
extern void Module_Init(void);
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
    MODULE_IDLE_TASK,
    MODULE_TASK_CNT
} Module_taskSpeeds_E;

typedef struct
{
    uint8_t  total_percentage;
    uint32_t iterations;
    uint16_t stack_left;
} Module_taskStats_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

float32_t Module_getTotalRuntimePercentage(Module_taskSpeeds_E task);
uint32_t  Module_getTotalRuntimeIterations(Module_taskSpeeds_E task);
uint8_t   Module_getMinStackLeft(Module_taskSpeeds_E task);

void Module_ApplicationIdleHook(void);
