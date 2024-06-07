/**
 * HW_can.h
 * Header file for the CAN hardware implementation
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"

#include "CAN/CanTypes.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CAN_mbFreeBus0(mb)    CAN_checkMbFree(&hcan, mb);


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void CAN_Start(void);
void HW_CAN_Init(void);

bool CAN_sendMsgBus0(CAN_TX_Priorities_E priority, CAN_data_T data, uint16_t id, uint8_t len);

bool CAN_getRxMessageBus0(CAN_RxFifo_E rxFifo, CAN_RxMessage_T *rx);
uint8_t CAN_getRxFifoFillLevelBus0(CAN_RxFifo_E rxFifo);
bool CAN_getRxFifoEmptyBus0(CAN_RxFifo_E rxFifo);

#ifdef __cplusplus
}
#endif
