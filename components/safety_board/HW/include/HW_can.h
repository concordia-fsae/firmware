/**
 * @file HW_can.h
 * @brief  Header file for CAN hardware peripheral
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2023-01-15
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SystemConfig.h"
#include "CAN/CAN_types.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CAN_mbFreeBus0(mb)    CAN_checkMbFree(&hcan, mb);


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void HW_CAN_Start(void);
void HW_CAN_Init(void);

bool CAN_sendMsgBus0(CAN_TX_Priorities_E priority, CAN_data_T data, uint16_t id, uint8_t len);

