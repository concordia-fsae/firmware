/**
 * CAN.h
 * Header file for CAN implementation
 */

#pragma once


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_can_componentSpecific.h"
#include "CAN/CanTypes.h"
#include "FreeRTOS_SWI.h"
#include "NetworkDefines_generated.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(FEATURE_CANRX_SWI)
// Receiver
extern RTOS_swiHandle_T *CANRX_swi;
extern void CANRX_SWI(void); // Prototype for SWI function
extern void CANRX_notify(CAN_bus_E bus, CAN_RxFifo_E rxFifo);
#endif


#if FEATURE_IS_ENABLED(FEATURE_CANTX_SWI)
// Transmitter
extern RTOS_swiHandle_T *CANTX_swi;
extern void CANTX_SWI(void);  // Prototype for SWI function
#endif

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void CANRX_unpackMessage(CAN_bus_E bus, uint32_t id, CAN_data_T* data);
