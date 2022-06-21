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
extern RTOS_swiHandle_T *CANRX_BUS_A_swi;
extern void CANRX_BUS_A_SWI(void); // Prototype for SWI function
extern void CANRX_BUS_A_notify(CAN_RxFifo_E rxFifo);

// Transmitter
extern RTOS_swiHandle_T *CANTX_BUS_A_10ms_swi;
extern void CANTX_BUS_A_10ms_SWI(void);  // Prototype for SWI function
