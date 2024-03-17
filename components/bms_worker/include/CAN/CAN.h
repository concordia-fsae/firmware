/**
 * CAN.h
 * Header file for CAN implementation
 */

#pragma once


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN/CanTypes.h"

#include "FreeRTOS_SWI.h"


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

// Transmitter
extern RTOS_swiHandle_T* CANTX_BUS_A_100Hz_swi;
extern RTOS_swiHandle_T* CANTX_BUS_A_10Hz_swi;
extern RTOS_swiHandle_T* CANTX_BUS_A_1Hz_swi;
extern void CANTX_BUS_A_100Hz_SWI(void);  // Prototype for SWI function
extern void CANTX_BUS_A_10Hz_SWI(void);  // Prototype for SWI function
extern void CANTX_BUS_A_1Hz_SWI(void);  // Prototype for SWI function
