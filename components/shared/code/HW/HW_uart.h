/**
 * @file HW_uart.h
 * @brief  Header file for UART firmware
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW.h"
#include "LIB_Types.h"
#include "HW_uart_componentSpecific.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern UART_HandleTypeDef huart[HW_UART_PORT_COUNT];

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

HW_StatusTypeDef_E HW_UART_init(void);
HW_StatusTypeDef_E HW_UART_init_componentSpecific(void);
HW_StatusTypeDef_E HW_UART_deInit(void);
HW_StatusTypeDef_E HW_UART_startDMARX(HW_UART_port_E huart, uint32_t* data, uint32_t size);
