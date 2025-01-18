/**
 * @file HW_can.h
 * @brief  Header file for CAN firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "HW.h"
#include "HW_can_componentSpecific.h"

// Firmware Includes
#include "CAN/CanTypes.h"
#include "NetworkDefines_generated.h"

#if MCU_STM32_PN == FDEFS_STM32_PN_STM32F103XB
#define CAN_FILTERBANK_LENGTH 28U
#else
#error "No other MCU supported."
#endif
/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HW_StatusTypeDef_E HW_CAN_init(void);
HW_StatusTypeDef_E HW_CAN_deInit(void);
HW_StatusTypeDef_E HW_CAN_start(CAN_bus_E bus);
HW_StatusTypeDef_E HW_CAN_stop(CAN_bus_E bus);
HW_StatusTypeDef_E HW_CAN_sendMsgOnPeripheral(CAN_bus_E bus, CAN_TxMessage_T msg);
void               HW_CAN_activateFifoNotifications(CAN_bus_E bus, CAN_RxFifo_E rxFifo);
bool               HW_CAN_sendMsg(CAN_bus_E bus, CAN_data_T data, uint32_t id, uint8_t len);
bool               HW_CAN_getRxMessage(CAN_bus_E bus, CAN_RxFifo_E rxFifo, CAN_RxMessage_T* rx);
void               HW_CAN_TxComplete_ISR(CAN_bus_E bus, CAN_TxMailbox_E mailbox);
void               HW_CAN_RxMsgPending_ISR(CAN_bus_E bus, CAN_RxFifo_E fifoId);
void               HW_CAN_TxError_ISR(CAN_bus_E bus, CAN_TxMailbox_E mailbox);
