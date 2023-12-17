/**
 * @file FreeRTOS_types.h
 * @brief  Types relating to FreeRTOS
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "stdbool.h"

// FreeRTOS Includes
#include "FreeRTOS.h"
// #include "cmsis_os.h"
#include "event_groups.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    void (*function)(void);    // function to be executed for task
    const char* const   name;
    StackType_t* const  stack;
    const uint32_t      stackSize;
    void* const         parameters;
    const UBaseType_t   priority;
    StaticTask_t* const stateBuffer;
    TaskHandle_t        handle;
    const uint32_t      periodMs;
    uint32_t            timeSinceLastTickMs;
    struct
    {
        EventGroupHandle_t* const group;
        const EventBits_t         bit;
    } event;
} RTOS_taskDesc_t;
