/**
 * @file rtos_componentSpecific.c
 * @brief  Component-specific RTOS configuration for VCPDU
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "FreeRTOS_types.h"
#include "Utility.h"
#include "crashSensor.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static StaticTask_t crashSensorTask;
static StackType_t  crashSensorStack[configMINIMAL_STACK_SIZE];

static RTOS_taskDesc_t componentFreerunTasks[] = {
    {
        .function    = &crashSensor_task,
        .name        = "Crash Sensor",
        .priority    = 9U,
        .parameters  = NULL,
        .stack       = crashSensorStack,
        .stackSize   = sizeof(crashSensorStack) / sizeof(StackType_t),
        .stateBuffer = &crashSensorTask,
    },
};

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

uint16_t RTOS_getComponentFreerunTasks(RTOS_taskDesc_t** tasks);
uint16_t RTOS_getComponentFreerunTasks(RTOS_taskDesc_t** tasks)
{
    *tasks = componentFreerunTasks;
    return (uint16_t)COUNTOF(componentFreerunTasks);
}
