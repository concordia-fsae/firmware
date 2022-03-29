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

#define RTOS_SWI_PRI_OFFSET     12U
#define RTOS_SWI_MAX_PER_PRI    RTOS_EVENT_COUNT

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

typedef void (*RTOS_swiFn_t)(void);

typedef struct
{
    RTOS_swiFn_t handler;
    uint32_t     priority : 3;
    uint32_t     event    : 24;
} RTOS_swiHandle_T;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void RTOS_SWI_Init(void);

RTOS_swiHandle_T* RTOS_swiCreate(RTOS_swiPri_E priority, RTOS_swiFn_t handler);
void RTOS_swiInvoke(RTOS_swiHandle_T* handle);
bool RTOS_swiInvokeFromISR(RTOS_swiHandle_T* handle);
void RTOS_swiDisable(void);
void RTOS_swiEnable(void);


void RTOS_getSwiTaskmemory(RTOS_swiPri_E swiPriority,
                           StaticTask_t **ppxSwiTaskTCBBuffer,
                           StackType_t **ppxSwiTaskStackBuffer,
                           uint32_t *pusSwiTaskStackSize);


