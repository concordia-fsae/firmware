#pragma once

#include "HW.h"
#include "LIB_Types.h"

// Production UART port enums are component-specific; this enum preserves their ABI.
typedef enum { RIG_HW_UART_PORT_UNUSED = 0U, } HW_UART_port_E;

HW_StatusTypeDef_E HW_UART_startDMARX(HW_UART_port_E port, uint32_t* data, uint32_t size);
HW_StatusTypeDef_E HW_UART_stopDMA(HW_UART_port_E port);
uint32_t           HW_UART_getDmaRxRemaining(HW_UART_port_E port);

// Stop DMA and mask the UART interrupt source.
void               HW_UART_disable(HW_UART_port_E port);
