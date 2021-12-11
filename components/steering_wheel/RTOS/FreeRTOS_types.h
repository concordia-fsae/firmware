/*
 * FreeRTOS_types.h
 */

// system includes
#include "stdbool.h"

// other includes
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "event_groups.h"

typedef struct
{
    void (*function)(void); // function to be executed for task
    void * const        parameters;
    osThreadId_t        handle;
    osThreadAttr_t      attr;
    struct
    {
        EventGroupHandle_t* const group;
        const EventBits_t         bit;
    } event;
    const uint32_t periodMs;
    uint32_t       timeSinceLastTickMs;
} RTOS_taskDesc_t;
