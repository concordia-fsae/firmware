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

// Receiver
extern RTOS_swiHandle_T *CANRX_swi;
extern void CANRX_SWI(void); // Prototype for SWI function
extern void CANRX_notify(CAN_bus_E bus, CAN_RxFifo_E rxFifo);

// Transmitter
extern RTOS_swiHandle_T *CANTX_swi;
extern void CANTX_SWI(void);  // Prototype for SWI function
