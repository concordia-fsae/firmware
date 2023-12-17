/*
 * FreeRTOS_types.h
 */

#pragma once

// system includes
#include "stdbool.h"

// other includes
#include "FreeRTOS.h"
// #include "cmsis_os.h"
#include "event_groups.h"

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
