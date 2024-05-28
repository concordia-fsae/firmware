/**
 * FreeRTOS_SWI.h
 * Header file for FreeRTOS Software Interrupt implementation
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "FreeRTOSConfig.h"
#include "FreeRTOS_types.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define RTOS_SWI_PRI_OFFSET     12U              // SWI priority offset from other interrupts
#define RTOS_SWI_MAX_PER_PRI    RTOS_EVENT_COUNT // number of SWIs allowed per priority level

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    RTOS_SWI_PRI_0 = 0U,
    RTOS_SWI_PRI_1,
    RTOS_SWI_PRI_2,
    RTOS_SWI_PRI_COUNT,
} RTOS_swiPri_E;

// function pointer type
typedef void (*RTOS_swiFn_t)(void);

typedef struct
{
    RTOS_swiFn_t handler;                     // function to be called when SWI runs
    uint32_t     priority : 2;                // priority of this SWI
    uint32_t     event    : RTOS_EVENT_COUNT; // bitmask for the event bit for this SWI
} RTOS_swiHandle_T;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void RTOS_SWI_Init(void);

RTOS_swiHandle_T* SWI_create(RTOS_swiPri_E priority, RTOS_swiFn_t handler);

void SWI_invoke(RTOS_swiHandle_T* handle);
bool SWI_invokeFromISR(RTOS_swiHandle_T* handle);
void SWI_disable(void);
void SWI_enable(void);


void RTOS_getSwiTaskMemory(RTOS_swiPri_E swiPriority,
                           StaticTask_t **ppxSwiTaskTCBBuffer,
                           StackType_t **ppxSwiTaskStackBuffer,
                           uint32_t *pusSwiTaskStackSize);


