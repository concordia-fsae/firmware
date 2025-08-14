/**
 * @file Module.h
 * @brief  Header file for Module Manager
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "LIB_FloatTypes.h"
#include "LIB_Types.h"

// Other Includes
#include "Module_componentSpecific.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define set_taskUsage1kHz(m,b,n,s)               set(m,b,n,s, Module_getTotalRuntimePercentage(MODULE_1kHz_TASK));
#define set_taskUsage100Hz(m,b,n,s)              set(m,b,n,s, Module_getTotalRuntimePercentage(MODULE_100Hz_TASK));
#define set_taskUsage10Hz(m,b,n,s)               set(m,b,n,s, Module_getTotalRuntimePercentage(MODULE_10Hz_TASK));
#define set_taskUsage1Hz(m,b,n,s)                set(m,b,n,s, Module_getTotalRuntimePercentage(MODULE_1Hz_TASK));
#define set_taskIterations1kHz(m,b,n,s)          set(m,b,n,s, (uint16_t)Module_getTotalRuntimeIterations(MODULE_1kHz_TASK));
#define set_taskIterations100Hz(m,b,n,s)         set(m,b,n,s, (uint16_t)Module_getTotalRuntimeIterations(MODULE_100Hz_TASK));
#define set_taskIterations10Hz(m,b,n,s)          set(m,b,n,s, (uint16_t)Module_getTotalRuntimeIterations(MODULE_10Hz_TASK));
#define set_taskIterations1Hz(m,b,n,s)           set(m,b,n,s, (uint16_t)Module_getTotalRuntimeIterations(MODULE_1Hz_TASK));

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

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
} Module_taskStats_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void      Module_componentSpecific_Init(void);
void      Module_componentSpecific_1kHz(void);
void      Module_componentSpecific_100Hz(void);
void      Module_componentSpecific_10Hz(void);
void      Module_componentSpecific_1Hz(void);

float32_t Module_getTotalRuntimePercentage(Module_taskSpeeds_E task);
uint32_t  Module_getTotalRuntimeIterations(Module_taskSpeeds_E task);
void      Module_ApplicationIdleHook(void);
