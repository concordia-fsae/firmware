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
#include "FeatureDefines_generated.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

#if FEATURE_CANRX_SWI
// Receiver
extern RTOS_swiHandle_T* CANRX_BUS_VEH_swi;
extern void CANRX_BUS_VEH_SWI(void);  // Prototype for SWI function
extern void CANRX_BUS_VEH_notify(CAN_RxFifo_E rxFifo);
#endif // FEATURE_CANRX_SWI
#if FEATURE_CANTX_SWI
// Transmitter
extern RTOS_swiHandle_T* CANTX_BUS_VEH_swi;
#endif // FEATURE_CANTX_SWI
extern void CANTX_BUS_VEH_SWI(void);  // Prototype for SWI function
