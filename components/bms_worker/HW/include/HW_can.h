/**
 * @file HW_can.h
 * @brief  Header file for CAN firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW.h"
#include "SystemConfig.h"

// Firmware Includes
#include "CAN/CanTypes.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CAN_mbFreeBus0(mb) CAN_checkMbFree(&hcan, mb);


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HW_StatusTypeDef_E HW_CAN_start(void);
HW_StatusTypeDef_E HW_CAN_stop(void);
void    HW_CAN_activateFifoNotifications(CAN_bus_E bus, CAN_RxFifo_E rxFifo);
bool HW_CAN_checkMbFree(CAN_bus_E bus, CAN_TxMailbox_E mailbox);

HW_StatusTypeDef_E HW_CAN_init(void);
HW_StatusTypeDef_E HW_CAN_deInit(void);

bool CAN_sendMsg(CAN_bus_E bus, CAN_TxMailbox_E mailbox, CAN_data_T data, uint16_t id, uint8_t len);

bool CAN_getRxMessage(CAN_bus_E bus, CAN_RxFifo_E rxFifo, CAN_RxMessage_T *rx);
uint8_t CAN_getRxFifoFillLevel(CAN_bus_E bus, CAN_RxFifo_E rxFifo);
bool    CAN_getRxFifoEmpty(CAN_bus_E bus, CAN_RxFifo_E rxFifo);
